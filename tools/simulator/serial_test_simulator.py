#!/usr/bin/env python3
"""
No-board serial test simulator for main-board security flow.

Usage:
  python tools/simulator/serial_test_simulator.py
  python tools/simulator/serial_test_simulator.py 900 101 310 300 208
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Optional, Tuple
import sys


MODE_DISARM = "disarm"
MODE_NIGHT = "night"
MODE_AWAY = "away"

LEVEL_OFF = "off"
LEVEL_WARN = "warn"
LEVEL_ALERT = "alert"

CMD_NONE = "none"
CMD_BUZZER_WARN = "buzzer_warn"
CMD_BUZZER_ALERT = "buzzer_alert"

SRC_GENERIC = 200
SRC_PIR1 = 201
SRC_PIR2 = 202
SRC_PIR3 = 203
SRC_US1 = 211
SRC_US2 = 212
SRC_US3 = 213

INTRUDER_EVENT_TRIGGERS = {
    "door_open",
    "window_open",
    "door_tamper",
    "vib_spike",
    "motion",
    "chokepoint",
    "entry_timeout",
}


@dataclass
class SimConfig:
    entry_delay_ms: int = 15000
    exit_grace_after_indoor_activity_ms: int = 30000
    outdoor_pir_src: int = 3
    correlation_window_ms: int = 20000
    suspicion_decay_step_ms: int = 5000
    suspicion_decay_points: int = 8


@dataclass
class SimState:
    mode: str = MODE_DISARM
    level: str = LEVEL_OFF
    door_locked: bool = True
    window_locked: bool = True
    door_open: bool = False
    window_open: bool = False
    keep_window_locked_when_disarmed: bool = False
    entry_pending: bool = False
    entry_deadline_ms: int = 0
    suspicion_score: int = 0
    last_suspicion_update_ms: int = 0
    last_indoor_activity_ms: int = 0
    last_outdoor_motion_ms: int = 0
    last_window_event_ms: int = 0
    last_vibration_ms: int = 0
    last_door_event_ms: int = 0
    serial_override_enabled: bool = False


CodeMap = Dict[int, Tuple[str, int, str]]

CODE_MAP: CodeMap = {
    100: ("disarm", SRC_GENERIC, "mode disarm"),
    101: ("arm_night", SRC_GENERIC, "mode night"),
    102: ("arm_away", SRC_GENERIC, "mode away"),
    200: ("manual_door_toggle", SRC_GENERIC, "manual door toggle"),
    201: ("manual_window_toggle", SRC_GENERIC, "manual window toggle"),
    202: ("manual_lock_request", SRC_GENERIC, "manual lock all"),
    203: ("manual_unlock_request", SRC_GENERIC, "manual unlock all"),
    204: ("door_hold_warn_silence", SRC_GENERIC, "silence hold warning"),
    205: ("keypad_help_request", SRC_GENERIC, "help request"),
    206: ("door_code_unlock", SRC_GENERIC, "door code unlock"),
    207: ("door_code_bad", SRC_GENERIC, "door code bad"),
    208: ("entry_timeout", SRC_GENERIC, "entry timeout"),
    300: ("door_open", SRC_GENERIC, "door open"),
    301: ("window_open", SRC_GENERIC, "window open"),
    302: ("door_tamper", SRC_GENERIC, "door tamper"),
    303: ("vib_spike", SRC_GENERIC, "vibration spike"),
    310: ("motion", SRC_PIR1, "motion pir1 zone a"),
    311: ("motion", SRC_PIR2, "motion pir2 zone b"),
    312: ("motion", SRC_PIR3, "motion pir3 outdoor"),
    320: ("chokepoint", SRC_US1, "chokepoint us1 door"),
    321: ("chokepoint", SRC_US2, "chokepoint us2 window"),
    322: ("chokepoint", SRC_US3, "chokepoint us3 between-room"),
    900: ("serial_test_enable", SRC_GENERIC, "enable serial override"),
    901: ("serial_test_disable", SRC_GENERIC, "disable serial override"),
}


def level_from_score(score: int) -> str:
    if score >= 45:
        return LEVEL_ALERT
    if score >= 15:
        return LEVEL_WARN
    return LEVEL_OFF


def add_score(st: SimState, points: int) -> None:
    st.suspicion_score = min(100, st.suspicion_score + points)


def apply_decay(st: SimState, cfg: SimConfig, now_ms: int) -> None:
    if st.last_suspicion_update_ms == 0:
        st.last_suspicion_update_ms = now_ms
        return
    if cfg.suspicion_decay_step_ms == 0 or cfg.suspicion_decay_points == 0:
        st.last_suspicion_update_ms = now_ms
        return

    elapsed = now_ms - st.last_suspicion_update_ms
    steps = elapsed // cfg.suspicion_decay_step_ms
    if steps <= 0:
        return
    decay = steps * cfg.suspicion_decay_points
    st.suspicion_score = max(0, st.suspicion_score - decay)
    st.last_suspicion_update_ms = now_ms


def within(now_ms: int, ref_ms: int, window_ms: int) -> bool:
    return ref_ms != 0 and (now_ms - ref_ms) <= window_ms


def reset_mode_state(st: SimState, mode: str, now_ms: int) -> None:
    st.mode = mode
    st.level = LEVEL_OFF
    st.entry_pending = False
    st.entry_deadline_ms = 0
    st.suspicion_score = 0
    st.last_suspicion_update_ms = now_ms
    st.last_outdoor_motion_ms = 0
    st.last_window_event_ms = 0
    st.last_vibration_ms = 0
    st.last_door_event_ms = 0
    st.keep_window_locked_when_disarmed = False
    if mode in (MODE_NIGHT, MODE_AWAY):
        st.door_locked = True
        st.window_locked = True


def normalize_motion_src(src: int) -> int:
    if src == SRC_PIR1:
        return 1
    if src == SRC_PIR2:
        return 2
    if src == SRC_PIR3:
        return 3
    return src


def line_kind_from_result(event_type: str, command: str, score: int) -> str:
    if event_type == "keypad_help_request":
        return "help"
    if event_type in ("serial_test_enable", "serial_test_disable"):
        return "status"
    if command == CMD_BUZZER_WARN:
        return "warn"
    if command == CMD_BUZZER_ALERT:
        return "intruder_alert" if score >= 80 else "alert"
    if event_type in ("disarm", "arm_night", "arm_away"):
        return "command_ack"
    return "event"


def line_message_preview(event_type: str, src: int, command: str, st: SimState) -> str:
    # Keep this aligned with tools/line_bridge high-level formatting.
    if event_type == "keypad_help_request":
        return f"[HELP] keypad assistance requested | mode={st.mode} | level={st.level}"
    if st.level == LEVEL_ALERT and event_type in INTRUDER_EVENT_TRIGGERS:
        return (
            f"[ALERT] possible intruder detected | trigger={event_type} | "
            f"mode={st.mode} | level={st.level}"
        )
    if event_type in ("serial_test_enable", "serial_test_disable"):
        return f"[STATUS] reason={event_type} | mode={st.mode} | level={st.level}"
    return (
        f"[EVENT] event={event_type} | src={src} | cmd={command} | "
        f"mode={st.mode} | level={st.level}"
    )


def handle_event(st: SimState, cfg: SimConfig, event_type: str, src: int, now_ms: int) -> Tuple[str, str]:
    apply_decay(st, cfg, now_ms)
    command = CMD_NONE
    notify = ""

    if event_type == "serial_test_enable":
        st.serial_override_enabled = True
        notify = "serial test override enabled"
        return command, notify
    if event_type == "serial_test_disable":
        st.serial_override_enabled = False
        notify = "serial test override disabled"
        return command, notify

    if event_type == "disarm":
        reset_mode_state(st, MODE_DISARM, now_ms)
        return command, notify
    if event_type == "arm_night":
        reset_mode_state(st, MODE_NIGHT, now_ms)
        return command, notify
    if event_type == "arm_away":
        reset_mode_state(st, MODE_AWAY, now_ms)
        return command, notify

    if event_type == "manual_lock_request":
        if st.door_open:
            notify = "manual lock rejected: door is open"
        elif st.window_open:
            notify = "manual lock rejected: window is open"
        else:
            st.door_locked = True
            st.window_locked = True
            st.keep_window_locked_when_disarmed = True
            notify = "manual lock accepted"
        return command, notify

    if event_type == "manual_unlock_request":
        if st.mode != MODE_DISARM:
            notify = "manual unlock blocked: disarm required"
        else:
            st.door_locked = False
            st.window_locked = False
            st.keep_window_locked_when_disarmed = False
            notify = "manual unlock accepted"
        return command, notify

    if event_type == "manual_door_toggle":
        if st.door_locked:
            if st.mode != MODE_DISARM:
                notify = "manual door unlock blocked: disarm required"
            else:
                st.door_locked = False
                notify = "manual door unlocked"
        else:
            if st.door_open:
                notify = "manual door lock rejected: door is open"
            else:
                st.door_locked = True
                notify = "manual door locked"
        return command, notify

    if event_type == "manual_window_toggle":
        if st.window_locked:
            if st.mode != MODE_DISARM:
                notify = "manual window unlock blocked: disarm required"
            else:
                st.window_locked = False
                st.keep_window_locked_when_disarmed = False
                notify = "manual window unlocked"
        else:
            if st.window_open:
                notify = "manual window lock rejected: window is open"
            else:
                st.window_locked = True
                st.keep_window_locked_when_disarmed = True
                notify = "manual window locked"
        return command, notify

    if event_type == "door_code_unlock":
        if st.mode != MODE_DISARM:
            reset_mode_state(st, MODE_DISARM, now_ms)
        st.door_locked = False
        st.window_locked = True
        st.keep_window_locked_when_disarmed = True
        notify = "door code accepted"
        return command, notify

    if event_type == "door_code_bad":
        notify = "door code bad"
        return command, notify

    if event_type == "door_hold_warn_silence":
        notify = "hold warning silenced"
        return command, notify

    if event_type == "keypad_help_request":
        notify = "HELP requested from keypad"
        return command, notify

    if event_type == "door_open":
        st.door_open = True
    if event_type == "window_open":
        st.window_open = True

    if event_type == "door_open" and st.door_locked:
        st.entry_pending = False
        st.entry_deadline_ms = 0
        st.last_door_event_ms = now_ms
        st.suspicion_score = 100
        st.level = LEVEL_ALERT
        command = CMD_BUZZER_ALERT
        return command, notify

    armed = st.mode in (MODE_NIGHT, MODE_AWAY)

    if armed and event_type == "door_open":
        if not st.entry_pending:
            if not (
                st.last_indoor_activity_ms != 0
                and (now_ms - st.last_indoor_activity_ms) <= cfg.exit_grace_after_indoor_activity_ms
            ):
                st.entry_pending = True
                st.entry_deadline_ms = now_ms + cfg.entry_delay_ms
                st.last_door_event_ms = now_ms
                add_score(st, 15)
                st.level = level_from_score(st.suspicion_score)
                command = CMD_BUZZER_WARN
        return command, notify

    if armed and event_type == "entry_timeout":
        st.entry_pending = False
        st.entry_deadline_ms = 0
        st.suspicion_score = 100
        st.level = LEVEL_ALERT
        command = CMD_BUZZER_ALERT
        return command, notify

    if armed and event_type == "window_open":
        st.last_window_event_ms = now_ms
        add_score(st, 40)
        if within(now_ms, st.last_outdoor_motion_ms, cfg.correlation_window_ms):
            add_score(st, 15)
        if within(now_ms, st.last_vibration_ms, cfg.correlation_window_ms):
            add_score(st, 10)
        st.level = level_from_score(st.suspicion_score)
        command = CMD_BUZZER_ALERT if st.level == LEVEL_ALERT else CMD_BUZZER_WARN
        return command, notify

    if armed and event_type in ("motion", "chokepoint"):
        motion_src = normalize_motion_src(src)
        indoor = (event_type == "chokepoint") or (event_type == "motion" and motion_src != cfg.outdoor_pir_src)
        if indoor:
            st.last_indoor_activity_ms = now_ms
            add_score(st, 18)
            if within(now_ms, st.last_window_event_ms, cfg.correlation_window_ms):
                add_score(st, 20)
            if within(now_ms, st.last_vibration_ms, cfg.correlation_window_ms):
                add_score(st, 12)
            if within(now_ms, st.last_door_event_ms, cfg.correlation_window_ms):
                add_score(st, 8)
        else:
            st.last_outdoor_motion_ms = now_ms
            add_score(st, 10)
        st.level = level_from_score(st.suspicion_score)
        command = CMD_BUZZER_ALERT if st.level == LEVEL_ALERT else CMD_BUZZER_WARN
        return command, notify

    if armed and event_type == "vib_spike":
        st.last_vibration_ms = now_ms
        add_score(st, 22)
        if within(now_ms, st.last_outdoor_motion_ms, cfg.correlation_window_ms):
            add_score(st, 12)
        if within(now_ms, st.last_window_event_ms, cfg.correlation_window_ms):
            add_score(st, 10)
        st.level = level_from_score(st.suspicion_score)
        if st.suspicion_score >= 80:
            st.entry_pending = False
            st.entry_deadline_ms = 0
        command = CMD_BUZZER_ALERT if st.level == LEVEL_ALERT else CMD_BUZZER_WARN
        return command, notify

    if armed and event_type == "door_tamper":
        add_score(st, 65)
        if within(now_ms, st.last_outdoor_motion_ms, cfg.correlation_window_ms):
            add_score(st, 15)
        st.level = level_from_score(st.suspicion_score)
        if st.suspicion_score >= 80:
            st.entry_pending = False
            st.entry_deadline_ms = 0
        command = CMD_BUZZER_ALERT
        return command, notify

    st.level = level_from_score(st.suspicion_score)
    return command, notify


def print_help() -> None:
    print("No-board serial simulator")
    print("Input a code, '?' for list, '+ms' to advance time, 'exit' to quit")
    print("Examples: 900 101 310 300 +16000 208")
    print("")
    for code in sorted(CODE_MAP.keys()):
        event_type, src, desc = CODE_MAP[code]
        print(f"{code:>3}  {event_type:<22} src={src:<3} {desc}")


def print_trace(code: int, event_type: str, src: int, command: str, st: SimState, now_ms: int) -> None:
    line_kind = line_kind_from_result(event_type, command, st.suspicion_score)
    line_message = line_message_preview(event_type, src, command, st)
    print(f"[TRACE] time.ms={now_ms}")
    print(f"[TRACE] input.code={code}")
    print(f"[TRACE] event.type={event_type}")
    print(f"[TRACE] event.src={src}")
    print(f"[TRACE] command.type={command}")
    print(f"[TRACE] state.mode={st.mode}")
    print(f"[TRACE] state.level={st.level}")
    print(f"[TRACE] state.score={st.suspicion_score}")
    print(f"[TRACE] state.entry_pending={1 if st.entry_pending else 0}")
    print(f"[TRACE] output.door_locked={1 if st.door_locked else 0}")
    print(f"[TRACE] output.window_locked={1 if st.window_locked else 0}")
    print(f"[TRACE] output.door_open={1 if st.door_open else 0}")
    print(f"[TRACE] output.window_open={1 if st.window_open else 0}")
    print(f"[TRACE] line.kind={line_kind}")
    print(f"[TRACE] line.message={line_message}")
    print("---")


def handle_token(token: str, st: SimState, cfg: SimConfig, now_ms: int) -> int:
    t = token.strip()
    if not t:
        return now_ms
    if t in ("?", "help"):
        print_help()
        return now_ms
    if t.lower() in ("q", "quit", "exit"):
        raise EOFError
    if t.startswith("+") and t[1:].isdigit():
        delta = int(t[1:])
        now_ms += delta
        print(f"[TRACE] time advanced +{delta}ms -> {now_ms}")
        print("---")
        return now_ms
    if not t.isdigit():
        print(f"[SIM] unknown token: {t}")
        print("[SIM] use '?' for help")
        print("---")
        return now_ms

    code = int(t)
    mapping = CODE_MAP.get(code)
    if not mapping:
        print(f"[SIM] unknown code: {code}")
        print("[SIM] use '?' for help")
        print("---")
        return now_ms

    event_type, src, _ = mapping
    command, notify = handle_event(st, cfg, event_type, src, now_ms)
    print_trace(code, event_type, src, command, st, now_ms)
    return now_ms + 100


def main(argv: list[str]) -> int:
    st = SimState()
    cfg = SimConfig()
    now_ms = 0

    if len(argv) > 1:
        for token in argv[1:]:
            try:
                now_ms = handle_token(token, st, cfg, now_ms)
            except EOFError:
                break
        return 0

    print_help()
    while True:
        try:
            line = input("sim> ")
        except (EOFError, KeyboardInterrupt):
            print("")
            break
        for token in line.replace(",", " ").split():
            try:
                now_ms = handle_token(token, st, cfg, now_ms)
            except EOFError:
                return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
