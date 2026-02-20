# Requirements, Business Rules, and Acceptance Tests (Single-Board)

## 1. Scope

This release is constrained to one firmware target:

- `main-board` only

Legacy multi-board content is archived in `docs/legacy_2026-02-19/`.

## 2. Functional Requirements

| Req ID | Requirement | Mapped Business Rules |
|---|---|---|
| FR-01 | System shall support arming modes `night` and `away`, and `disarm`. | BR-01, BR-02, BR-03 |
| FR-02 | System shall evaluate intrusion events via deterministic suspicion scoring. | BR-06, BR-07, BR-08, BR-09, BR-10, BR-11 |
| FR-03 | System shall manage entry delay and timeout escalation when armed. | BR-04, BR-05 |
| FR-04 | System shall enforce safe lock/unlock behavior based on door/window physical state. | BR-12, BR-13, BR-21, BR-22 |
| FR-05 | System shall secure remote commands with token/nonce replay protection. | BR-17, BR-18, BR-19 |
| FR-06 | System shall apply fail-closed policy on sensor fault for unlock operations. | BR-20 |
| FR-07 | System shall apply keypad lockout after configured bad-attempt threshold. | BR-23 |
| FR-08 | System shall publish MQTT event/status/ack/metrics telemetry for operations and monitoring. | BR-25, BR-26, BR-27 |
| FR-09 | System shall keep mode continuity across reboot with validated persisted restore and safe fallback. | BR-29, BR-30 |

## 3. Non-Functional Requirements

| Req ID | Requirement | Mapped Business Rules |
|---|---|---|
| NFR-01 | Behavior shall be deterministic under same event sequence and config. | BR-10, BR-11 |
| NFR-02 | Security posture shall default to deny under auth/storage uncertainty. | BR-17, BR-18, BR-19, BR-20 |
| NFR-03 | Build target shall be single-board for submission baseline. | BR-28 |
| NFR-04 | Runtime shall provide periodic and event-driven observability via MQTT. | BR-25, BR-26, BR-27 |
| NFR-05 | Boot behavior shall be deterministic after power cycle and shall not arm on invalid persisted mode data. | BR-29, BR-30 |

## 4. Acceptance Test Matrix

| TC ID | Scenario | Steps | Expected Result | Rules |
|---|---|---|---|---|
| TC-01 | Disarm reset | Arm system, generate score, send `disarm` | Mode becomes disarm, score resets to 0, entry pending cleared | BR-01 |
| TC-02 | Entry timeout alert | In armed mode, trigger door open then wait entry timeout | Alarm becomes alert with score forced to 100; alert buzzer is emitted | BR-04, BR-05 |
| TC-03 | Window correlation scoring | In armed mode, trigger outdoor motion then window open within correlation window | Score increments include correlation bonus; level updated accordingly | BR-06, BR-11 |
| TC-04 | Vibration high-risk path | In armed mode, trigger events to push score >= 80 via vibration path | Entry pending is cleared while user-facing level remains alert | BR-08, BR-11 |
| TC-05 | Unlock timeout relock | Unlock door (disarm), do not open door | Door auto-locks at timeout and warning behavior follows policy | BR-12, BR-14 |
| TC-06 | Open-close relock | Unlock door, open then close door | Door auto-locks after close delay | BR-13 |
| TC-07 | Hold warning silence | Keep door open past hold threshold, then trigger silence event | Buzzer warning stops and session remains controlled | BR-15, BR-16 |
| TC-08 | Remote unauthorized reject | Send mutating remote cmd without valid token/nonce | ACK reject and status reject reason | BR-17, BR-18 |
| TC-09 | Nonce persistence fail-closed | Disable nonce persistence with strict mode, send mutating cmd | Command rejected due to nonce storage unavailability | BR-19 |
| TC-10 | Sensor fault unlock reject | Activate sensor-fault condition then issue unlock cmd | Unlock rejected and reason published | BR-20 |
| TC-11 | Lock precondition reject | Attempt lock door/window while sensor indicates open | Command rejected, actuator not locked | BR-21 |
| TC-12 | Keypad lockout | Enter bad code until limit then try valid code during lockout | Unlock denied until lockout expiry | BR-23 |
| TC-13 | Serial hardening | Inject serial mode/manual/sensor commands under default config | Commands blocked and block status published | BR-24 |
| TC-14 | Telemetry contract | Observe event/status payload in runtime | Payload contains single-board fields and no auto-board context dependency | BR-26, BR-27, BR-28 |
| TC-15 | Mode restore after reboot | Set mode to `away`, reboot board, observe first status after boot | Mode restored to `away`; score/entry state reset to baseline; boot status published | BR-29, BR-30 |
| TC-16 | Invalid persisted mode fallback | Corrupt persisted mode value (outside valid set), reboot board | System falls back to `disarm`; warning telemetry/message emitted; no auto-arm | BR-29, BR-30 |

## 5. Build Verification

- `python -m platformio run -e main-board`
