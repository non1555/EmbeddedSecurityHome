# Selected LINE Response Test Cases (No-Board)

This is a confident subset of test cases for fast validation without hardware.

Run each case with:

```bash
python tools/simulator/serial_test_simulator.py <codes...>
```

## Selected Cases

| TC | Goal | Codes | Expected State (end) | Expected Simulator Trace | Expected LINE Push Type |
|---|---|---|---|---|---|
| TC-01 | Keypad help path | `205` | `mode=disarm`, `level=off` | `line.kind=help` | `Help Request` |
| TC-02 | Armed motion warning | `102 310` | `mode=away`, `level=warn` | `line.kind=warn` | `System Event` |
| TC-03 | Armed tamper escalation | `102 302` | `mode=away`, `level=alert` | `line.kind=alert` | `Security Alert` |
| TC-04 | Forced-open while locked | `202 300` | `mode=disarm`, `level=alert`, `score=100` | `line.kind=intruder_alert` | `Security Alert` |
| TC-05 | Door open while unlocked in disarm | `203 300` | `mode=disarm`, `level=off` | `line.kind=event` | `System Event` |
| TC-06 | Away + window breach baseline | `102 301` | `mode=away`, `level=warn` | `line.kind=warn` | `System Event` |
| TC-07 | Away window then motion correlation | `102 301 310` | `mode=away`, `level=alert` | `line.kind=alert` | `Security Alert` |
| TC-08 | Mode transition acknowledgement | `100 102 100` | `mode=disarm`, `level=off` | `line.kind=command_ack` | `System Event` |

## Notes

- These cases are selected to be deterministic and easy to explain in demo.
- For exact line text formatting, final source is `tools/line_bridge/bridge.py`.
- For full code list, see `docs/serial_test_codes.md`.
