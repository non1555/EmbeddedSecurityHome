# Business Rules (Single-Board)

## Context

This system runs on one firmware target: `main-board`.
All rules below describe runtime behavior of the main security controller and its MQTT/LINE integration.

## Rule Definitions

### Mode and Risk Rules

| ID | Rule | Trigger | Required Behavior |
|---|---|---|---|
| BR-01 | Disarm reset | `disarm` event | Set mode to `disarm`, clear entry pending, reset suspicion score to 0, clear recent correlation timestamps. |
| BR-02 | Arm night reset | `arm_night` event | Set mode to `night`, clear entry pending, reset suspicion score and correlation timestamps. |
| BR-03 | Arm away reset | `arm_away` event | Set mode to `away`, clear entry pending, reset suspicion score and correlation timestamps. |
| BR-04 | Forced door-open alert | `door_open` while `door_locked=true` (any mode) | Treat as immediate intrusion: set level to `alert`, set suspicion to 100, clear entry pending, and trigger alert buzzer output. |
| BR-04A | Armed entry warning gate | `door_open` while armed and door not locked | Start entry delay only if no current entry is pending and no indoor activity is within grace window. |
| BR-05 | Entry timeout escalation | `entry_timeout` while armed | Set alarm level to `alert`, set suspicion to 100, end entry pending, and trigger alert buzzer output. |
| BR-06 | Window breach scoring | `window_open` while armed | Increase suspicion score strongly and apply correlation bonus from outdoor motion/vibration windows. |
| BR-07 | Motion/chokepoint scoring | `motion` or `chokepoint` while armed | Score indoor/outdoor events differently, with correlation boosts from recent door/window/vibration activity. |
| BR-08 | Vibration escalation | `vib_spike` while armed | Increase suspicion and correlations; if score enters high-risk band (>=80), clear entry pending while keeping user-facing level as `alert`. |
| BR-09 | Tamper escalation | `door_tamper` while armed | Add high suspicion and trigger buzzer alert; if score enters high-risk band (>=80), clear entry pending. |
| BR-10 | Suspicion decay | On each decision cycle | Decay suspicion over time by configured step and points; level must reflect decayed score. |
| BR-11 | Level thresholds | Any score update | Map score to levels: `off` (<15), `warn` (15-44), `alert` (>=45). |

### Door Unlock Session Rules

| ID | Rule | Trigger | Required Behavior |
|---|---|---|---|
| BR-12 | Unlock timeout relock | Door unlock session active, door never opened | Auto-lock door when unlock timeout expires. |
| BR-13 | Open-then-close relock | Door unlock session active, door opened then closed | Auto-lock door after configured close delay. |
| BR-14 | Pre-expiry warning | Door unlock session active before timeout | Warn buzzer periodically in the warning window before timeout. |
| BR-15 | Door-open hold warning | Door remains open after hold threshold | Warn buzzer repeatedly until silence request or door state change. |
| BR-16 | Hold warning silence | `door_hold_warn_silence` event under active hold warning | Stop buzzer and mark hold warning as silenced. |

### Command and Safety Rules

| ID | Rule | Trigger | Required Behavior |
|---|---|---|---|
| BR-17 | Remote auth required | Remote command received | Mutating commands require token and nonce policy when configured; unauthorized payloads must be rejected. |
| BR-18 | Replay protection | Remote mutating command with nonce | Reject replayed/expired nonce; persist monotonic floor when enabled. |
| BR-19 | Fail-closed nonce storage | Mutating remote command + strict persistence mode + no nonce storage | Reject command with explicit auth failure reason. |
| BR-20 | Fail-closed sensor fault | Unlock command while sensor-fault fail-closed is active | Reject unlock operations and publish rejection status. |
| BR-21 | Physical lock preconditions | `lock door` / `lock window` / `lock all` | Reject lock operations if corresponding opening sensor reports open. |
| BR-22 | Unlock mode gate | Manual/remote unlock while mode is armed | Reject unlock unless mode is `disarm`. |
| BR-23 | Keypad bad-attempt lockout | Consecutive bad code attempts reaches limit | Enter keypad lockout period and block unlock by keypad during lockout. |
| BR-24 | Serial command hardening | Serial synthetic events | Block mode/manual/sensor serial commands unless explicitly enabled by config flags. |

### Boot and Persistence Rules

| ID | Rule | Trigger | Required Behavior |
|---|---|---|---|
| BR-29 | Boot mode baseline | Controller boot sequence | Start with baseline mode `disarm`; do not auto-enter armed mode without explicit command or valid persisted mode restore. |
| BR-30 | Mode persistence and restore | Mode transition accepted or next boot | Persist mode only when it changes; on boot, restore only valid persisted values (`disarm`/`night`/`away`), otherwise fallback to `disarm` and emit warning telemetry. |

### Telemetry and Contract Rules

| ID | Rule | Trigger | Required Behavior |
|---|---|---|---|
| BR-25 | Status heartbeat | Runtime loop | Publish periodic status snapshot by configured heartbeat interval. |
| BR-26 | Event telemetry | Event accepted/handled | Publish event payload with event/cmd/level/lock/open fields and timestamp. |
| BR-27 | Status telemetry | State update/reason update | Publish status payload with reason/level/lock/open fields and uptime. |
| BR-28 | Single-board payload contract | Main-board MQTT output | No auto-board context fields are required in active contract. |

## Operational Intent

- Prioritize fail-safe behavior under uncertainty (sensor fault, nonce persistence issues, unauthorized commands).
- Make boot behavior deterministic: `disarm` baseline plus strict validation for mode restore after power loss.
- Keep alarm severity deterministic via explicit score and threshold policy.
- Keep one user-facing high intrusion severity (`alert`) and avoid separate `critical` UI paths.
- Preserve operator observability through status/event/ack/metrics topics.

## Presentation Flowchart

- Source: `docs/business_rules_presentation_flowchart.mmd`
- Rendered SVG: `docs/business_rules_presentation_flowchart.svg`
- Purpose: high-level BR walkthrough for presentation, derived from `docs/full_system_flow_detailed.mmd`.

## Presentation Companion

- `docs/mode_event_response_matrix.md`: mode+event to action/LINE mapping in table form.
- `docs/line_response_selected_testcases.md`: selected no-board test cases for quick demo.
