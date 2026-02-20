# Mode-Event Response Matrix (Presentation)

This file is a presentation-friendly view of runtime behavior.
Source of truth remains `docs/business_rules.md`.

## Quick Mapping

| Mode | Event | Condition | Decision / State Result | Device Output | LINE Output Path |
|---|---|---|---|---|---|
| any | `disarm` | accepted command/event | set mode `disarm`, clear pending, reset risk | none | `System Event` |
| any | `arm_away` | accepted command/event | set mode `away`, clear pending, reset risk | none | `System Event` |
| any | `door_open` | `door_locked=true` | force `level=alert`, `score=100`, clear pending | `buzzer_alert` | `Security Alert` |
| `away` | `door_open` | `door_locked=false` and gate passed | start entry delay (`entry_pending=true`) | `buzzer_warn` | `System Event` |
| `away` | `entry_timeout` | timeout while armed | escalate to `level=alert`, `score=100` | `buzzer_alert` | `Security Alert` |
| `away` | `window_open` | armed | raise risk by rule score | `buzzer_warn` or `buzzer_alert` | `System Event` or `Security Alert` |
| `away` | `motion` / `chokepoint` | armed | raise risk by zone/correlation | `buzzer_warn` or `buzzer_alert` | `System Event` or `Security Alert` |
| `away` | `vib_spike` | armed | raise risk with correlation bonus | `buzzer_warn` or `buzzer_alert` | `System Event` or `Security Alert` |
| `away` | `door_tamper` | armed | fast raise risk, may clear pending | `buzzer_alert` | `Security Alert` |
| any | `keypad_help_request` | keypad `B` / test code | no mode change | none | `Help Request` |

## LINE Output Rule (Bridge)

Bridge behavior in `tools/line_bridge/bridge.py`:

- `Help Request`: event is `keypad_help_request`
- `Security Alert`: level is `alert` and trigger event is intrusion-related
- `System Event`: all normal event pushes
- `System Status`: status pushes
- `Command Result`: ack pushes (except `status` ack is suppressed to avoid spam)
