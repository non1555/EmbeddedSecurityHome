# Layer Structure

This project uses a layered firmware design on ESP32 with two RTOS tasks:

- `SecTask` (`20 ms`, priority `2`): drains remote events, polls keypad/local lock-unlock toggle buttons/sensors, checks timeout, processes `eventQueue`, updates actuators and OLED state.
- `MqttTask` (`10 ms`, priority `1`): runs Wi-Fi/MQTT reconnect, receives MQTT callbacks into `commandQueue`, and flushes `publishQueue`.

## Layer 1: Configuration & Shared Types

- `src/main_board/configuration_shared_types/Config.h`
  - Central constants for GPIO assignment, timing windows, RTOS periods, ultrasonic timeout, and door-session timing.
- `src/main_board/configuration_shared_types/Types.h`
  - Shared enums and structs used across the whole firmware (`Mode`, `AlarmLevel`, `EventType`, `Event`, `SystemState`, queue payload structs).
- `src/main_board/configuration_shared_types/RuntimeStats.h`
  - Runtime counters for dropped events / queue pressure telemetry.

## Layer 2: Hardware Abstraction Layer

Rule: this layer talks to hardware only. It does not decide security policy.

- `src/main_board/hardware_abstraction_layer/Sensors.h` / `Sensors.cpp`
  - Reads reed switches, PIR sensors, vibration sensor, and ultrasonic sensors.
  - Ultrasonic inputs are staggered with round-robin polling and `pulseIn()` timeout.
  - Does not parse user commands from Serial; input events come from hardware polling only.
- `src/main_board/hardware_abstraction_layer/Actuators.h` / `Actuators.cpp`
  - Drives door servo, window servo, and buzzer patterns.
- `src/main_board/hardware_abstraction_layer/KeypadController.h` / `KeypadController.cpp`
  - Scans the keypad and manages the PIN buffer.
  - `*` = backspace, `C` = clear, `#` = submit, `A` = silence, `B` = help.
- `src/main_board/hardware_abstraction_layer/DisplayManager.h` / `DisplayManager.cpp`
  - Updates the OLED with PIN feedback, door state, and countdown hints.

## Layer 3: Core Services

- `src/main_board/core_services/MqttService.h` / `MqttService.cpp`
  - Handles Wi-Fi/MQTT connectivity.
  - Runs inside `MqttTask`, not inside `SecTask`.
  - Receives incoming MQTT payloads into `commandQueue`.
  - Flushes outgoing event/status/ack messages from `publishQueue`.
- `src/main_board/core_services/NvsStorage.h` / `NvsStorage.cpp`
  - Persists and restores `latest_mode` and `is_night` for power-loss recovery.

## Layer 4: Application Logic Layer

- `src/main_board/application_logic_layer/EventCollector.h` / `EventCollector.cpp`
  - Converts local inputs into normalized events.
  - Poll order is:
    1. keypad
    2. local lock/unlock toggle buttons
    3. sensor chain
  - Local lock/unlock toggle buttons are debounced and emitted as `manual_door_toggle` / `manual_window_toggle`.
- `src/main_board/application_logic_layer/RuleEngine.h` / `RuleEngine.cpp`
  - Main state machine for mode transitions, warnings, alerts, and auto-arm logic.
  - Local lock/unlock toggle events are routed directly to `SystemContext`.
  - Auto-arm tick logic is evaluated from `updateActuators()`, including stage `1/2` timeout reset and stage `3` transition to `Away`.
- `src/main_board/application_logic_layer/SystemContext.h` / `SystemContext.cpp`
  - Owns the canonical system state.
  - Applies decisions, enforces remote-command policy, manages door session timers, persists mode state, and queues MQTT publish messages.
  - Handles local physical lock/unlock toggles, remote `keypad_help`, and warning rejection when trying to lock while a door/window is still open.
  - Publishes periodic status plus concise reason-tagged status/event messages for the LINE bridge.

## Entry Point

- `src/main_board/main.cpp`
  - Boot flow:
    1. start serial
    2. initialize task watchdog
    3. initialize `SystemContext`
    4. initialize `EventCollector`
    5. create `eventQueue`
    6. create `SecTask` and `MqttTask`
  - Emits Serial runtime trace for events, state changes, and publish reasons; it is not used as a control interface.
  - `loop()` stays idle because the firmware runs through RTOS tasks.
