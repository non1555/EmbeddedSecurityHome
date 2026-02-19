# Project Description (Requirements, Details, Test Cases)

Status: working draft (as-built aligned, still evolving)
Last updated: 2026-02-19

## 1) Project Overview

`EmbeddedSecurityHome` is an embedded home-security + automation system with:
- `Main Board` (ESP32): security orchestration, intrusion detection, lock and alarm control
- `Automation Board` (ESP32): light/fan automation using sensor data plus main-board context
- `LINE Bridge` (Python): LINE webhook + MQTT bridge for command and notifications

Primary goals:
- detect suspicious activity in armed mode
- notify user through LINE with actionable information
- enforce fail-closed behavior for mutating remote commands
- keep local automation separated from security-critical logic

## 2) Requirements

### 2.1 Functional Requirements

| ID | Requirement |
|---|---|
| FR-01 | System shall support security modes: `startup_safe`, `disarm`, `away`, `night`. |
| FR-02 | System shall detect events from reed, PIR, vibration, ultrasonic chokepoint, keypad, and manual buttons. |
| FR-03 | System shall evaluate intrusion risk using rule-based scoring and produce `off/warn/alert/critical`. |
| FR-04 | System shall publish MQTT topics for `event`, `status`, `ack`, and `metrics`. |
| FR-05 | Remote mutating commands shall require token + nonce (anti-replay) unless explicitly configured otherwise. |
| FR-06 | Lock/unlock actions shall obey policy checks (mode, sensor-fault fail-closed, open-contact constraints). |
| FR-07 | Door unlock session shall support timeout, warning, and auto-relock logic. |
| FR-08 | Automation board shall run light/fan auto control with hysteresis and main-context gating. |
| FR-09 | LINE bridge shall relay MQTT status/event/ack to LINE target. |
| FR-10 | When suspicious activity reaches high severity, bridge shall send explicit intruder alert to LINE. |
| FR-11 | Keypad shall provide a help-request trigger and system shall notify via MQTT/LINE. |
| FR-12 | System shall keep manual and automation control paths operational under normal MQTT and device runtime. |

### 2.2 Non-Functional Requirements

| ID | Requirement |
|---|---|
| NFR-01 | Security-critical paths should fail closed when command auth prerequisites are missing. |
| NFR-02 | Repeated notifications should be throttled/cooldown-limited to reduce spam. |
| NFR-03 | Core logic should be modularized by board responsibility (security vs automation vs bridge). |
| NFR-04 | Firmware shall build for both `main-board` and `automation-board` using PlatformIO. |
| NFR-05 | Bridge code should be testable by local unit tests without requiring live MQTT/LINE. |

## 3) Current Functional Details (As-Built)

### 3.1 Main Board Security Logic

- Event pipeline:
  - `remote cmd -> keypad -> timeout -> sensor/manual/serial`
- Rule engine:
  - event scoring and correlation
  - state transitions and command derivation (`none`, `buzzer_warn`, `buzzer_alert`, `notify`, `servo_lock`)
- Outputs:
  - servo lock/unlock
  - buzzer warn/alert/stop
  - MQTT `event/status/ack/metrics`

Key files:
- `src/main_board/app/SecurityOrchestrator.cpp`
- `src/main_board/app/RuleEngine.cpp`
- `src/main_board/services/CommandDispatcher.cpp`

### 3.2 Automation Board Logic

- Light auto:
  - lux hysteresis (`LUX_ON/LUX_OFF`)
  - gated by main-mode and main-presence context
- Fan auto:
  - temp hysteresis (`FAN_ON_C/FAN_OFF_C`)
  - same gating policy as light
- MQTT:
  - parses `main status` context and auto commands
  - publishes auto status and auto ack

Key files:
- `src/auto_board/app/AutomationRuntime.cpp`
- `src/auto_board/automation/light_system.cpp`
- `src/auto_board/automation/temp_system.cpp`

### 3.3 LINE Bridge Logic

- Command ingress:
  - LINE webhook or HTTP `/cmd`
  - supports lock/unlock/status command set
  - secure payload format: `token|nonce|cmd` when token configured
- MQTT egress to LINE:
  - pushes event/status/ack/metrics summaries
  - suppresses `ack(status)` spam

Key file:
- `tools/line_bridge/bridge.py`

### 3.4 Latest Behavior Update

1. Intruder-specific LINE alert
- Bridge now emits an explicit message:
  - `[ALERT] possible intruder detected`
- Trigger condition:
  - level in `{alert, critical}`
  - event/reason in intrusion trigger set
- Cooldown:
  - `INTRUDER_NOTIFY_COOLDOWN_S` (default 20s)

2. Keypad help request
- Keypad key `B` generates `keypad_help_request`
- Main board publishes event/status with reason `keypad_help_request`
- Bridge emits:
  - `[HELP] keypad assistance requested`

## 4) Test Strategy

Test levels:
- Unit test (bridge logic)
- Build/compile validation (both firmware environments)
- Functional integration checks (MQTT event/status pathways)
- Manual hardware scenario tests (field verification on sensors/actuators)

## 5) Test Cases

### 5.1 Security and Command Path

| TC ID | Requirement | Precondition | Steps | Expected Result | Method |
|---|---|---|---|---|---|
| TC-CMD-001 | FR-05 | token configured | send valid mutating command with valid nonce | command accepted, ack ok | integration |
| TC-CMD-002 | FR-05 | token configured | replay same nonce | command rejected, auth/replay fail | integration |
| TC-CMD-003 | FR-06 | mode != disarm | send `unlock door` | unlock rejected (`disarm required`) | integration |
| TC-CMD-004 | FR-06 | door open | send `lock door` | lock rejected (`door open`) | integration |
| TC-RULE-001 | FR-03 | mode armed | generate `door_open` | entry pending + warn path | unit/integration |
| TC-RULE-002 | FR-03, FR-10 | mode armed + timeout | trigger `entry_timeout` | critical level + notify/alert path | unit/integration |

### 5.2 Keypad and Help Request

| TC ID | Requirement | Precondition | Steps | Expected Result | Method |
|---|---|---|---|---|---|
| TC-KP-001 | FR-11 | keypad connected | press `B` | event `keypad_help_request` is produced | hardware/manual |
| TC-KP-002 | FR-11 | bridge connected | press `B` then observe LINE | receives `[HELP] keypad assistance requested` | integration/manual |
| TC-KP-003 | FR-07 | active door hold warning | press `A` | hold warning silenced | hardware/manual |

### 5.3 LINE Alerting

| TC ID | Requirement | Precondition | Steps | Expected Result | Method |
|---|---|---|---|---|---|
| TC-LINE-001 | FR-09 | bridge + line target ready | publish normal status/event | LINE receives formatted status/event text | integration |
| TC-LINE-002 | FR-10 | bridge connected | publish intrusion event with level=alert | LINE receives `[ALERT] possible intruder detected` | integration |
| TC-LINE-003 | NFR-02 | cooldown active | publish repeated intrusion events rapidly | alert messages are throttled by cooldown | integration |

### 5.4 Automation

| TC ID | Requirement | Precondition | Steps | Expected Result | Method |
|---|---|---|---|---|---|
| TC-AUTO-001 | FR-08 | light auto on + valid context | vary lux across thresholds | light toggles by hysteresis | hardware/manual |
| TC-AUTO-002 | FR-08 | fan auto on + valid context | vary temp across thresholds | fan toggles by hysteresis | hardware/manual |
| TC-AUTO-003 | FR-08 | main gate blocked | keep fan/light auto on | outputs forced off by gate policy | integration/manual |

## 6) Executed Results (Current Workspace)

Executed successfully:
- `python -m platformio run -e main-board`
- `python -m platformio run -e automation-board`
- `python -m unittest test.bridge.test_bridge -v` (12 tests, all pass)
- `python -m py_compile tools/line_bridge/bridge.py`

Not fully covered in this workspace session:
- full hardware-in-loop validation for all physical sensors and actuators
- long-duration stability soak test with real network loss/recovery cycles

## 7) Open Items

- tune intrusion-alert trigger list and cooldown for real deployment noise profile
- define dedicated keypad UX text/icon for help request on OLED
- add automated tests for new bridge intruder/help formatting paths
- extend native tests for keypad help event branch in orchestrator

