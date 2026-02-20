# Serial Test Codes (Main Board)

Use these codes in Serial Monitor to inject events without real sensor activity.

- Baud rate: `115200`
- Recommended line ending: `Newline`
- Send `900` first to enable serial test overrides.
- Send `901` to restore default serial policy.

## Mode Codes

- `100` -> `disarm`
- `101` -> `arm_night`
- `102` -> `arm_away`

## Command and Control Codes

- `200` -> `manual_door_toggle`
- `201` -> `manual_window_toggle`
- `202` -> `manual_lock_request`
- `203` -> `manual_unlock_request`
- `204` -> `door_hold_warn_silence`
- `205` -> `keypad_help_request`
- `206` -> `door_code_unlock`
- `207` -> `door_code_bad`
- `208` -> `entry_timeout`
- `900` -> enable serial test policy override
- `901` -> disable serial test policy override

## Sensor Input Codes

- `300` -> `door_open`
- `301` -> `window_open`
- `302` -> `door_tamper`
- `303` -> `vib_spike`
- `310` -> `motion` (PIR1 / zone A)
- `311` -> `motion` (PIR2 / zone B)
- `312` -> `motion` (PIR3 / outdoor)
- `320` -> `chokepoint` (US1 / door)
- `321` -> `chokepoint` (US2 / window)
- `322` -> `chokepoint` (US3 / between-room)

## Serial Output Style

Runtime trace now prints event, command, mode, and output fields as separate lines:

- `[TRACE] event.type=...`
- `[TRACE] command.type=...`
- `[TRACE] state.mode=...`
- `[TRACE] output.door_locked=...`
- `[TRACE] line.kind=...` for LINE notification class (debug only)
- `[TRACE] line.message=...` preview text that LINE should receive in that case

User-facing notification is sent via MQTT -> LINE bridge, not via Serial text.

Common `line.kind` values:
- `event` generic event message
- `status` status-reason message
- `help` keypad help request
- `warn` warning-level event path
- `intruder_alert` alert-level intrusion path

## No Board Mode

When no ESP32 board is connected, run host-side simulator:

```bash
python tools/simulator/serial_test_simulator.py
```

Quick scenario example:

```bash
python tools/simulator/serial_test_simulator.py 900 101 310 300 +16000 208
```

## LINE Preview Test Cases

- Help request:
  `python tools/simulator/serial_test_simulator.py 205`
  -> `[TRACE] line.message=[HELP] keypad assistance requested ...`
- Intruder alert:
  `python tools/simulator/serial_test_simulator.py 101 310 300 +16000 208`
  -> `[TRACE] line.message=[ALERT] possible intruder detected ...`
- Forced-open while locked (any mode):
  `python tools/simulator/serial_test_simulator.py 202 300`
  -> `[TRACE] line.message=[ALERT] possible intruder detected | trigger=door_open ...`
- Generic event:
  `python tools/simulator/serial_test_simulator.py 101 310`
  -> `[TRACE] line.message=[EVENT] event=motion ...`

For presentation-ready selected cases, see:
- `docs/line_response_selected_testcases.md`
