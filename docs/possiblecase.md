# Possible Cases & Logic Conditions

## Category 1: Mode Transitions & Automation

### Case 1.1: Auto-Arm Success

- Condition: System is in `Disarm` mode -> `chk1` triggered -> door opened -> door closed -> no indoor/window activity for `20s`.
- Expected Result: `exit_stage` advances from `1` to `3`, then the system switches to `Away`, saves mode state to NVS, locks actuators, and publishes `mode_away`.

### Case 1.2: Auto-Arm Cancelled

- Condition: System is counting the `20s` exit window (`exit_stage == 3`) and detects `motion 1`, `motion 2`, `motion 3`, `chk2`, or `chk3`.
- Expected Result: Auto-arm is cancelled, `exit_stage` resets to `0`, and the system remains in `Disarm`.

### Case 1.3: Auto-Arm Stage Timeout Reset

- Condition: System enters auto-arm stage `1` or `2`, but the next expected step does not happen within `15s`.
- Expected Result: `exit_stage` resets back to `0` with flag `auto_arm_timeout_reset`, preventing stale partial sequences from auto-arming later.

### Case 1.4: Door Auto-Lock After Close

- Condition: Door was unlocked by PIN, remote command, or manual toggle, then closes and stays closed for `3s`.
- Expected Result: Door servo locks again and publishes status reason `auto_locked`.

### Case 1.5: Door Unlock Timeout

- Condition: Door was unlocked but never opened, and the unlock session reaches `15s`.
- Expected Result: Door servo locks again and publishes status reason `auto_locked_timeout`.

### Case 1.6: Power Loss Recovery

- Condition: ESP32 restarts after power loss.
- Expected Result: System restores `latest_mode` and `is_night` from NVS, rebuilds active mode, starts in a locked state, and publishes `boot`.

## Category 6: Local Debug Interface

### Case 6.1: Serial Debug Help

- Condition: User types `help` or `?` in Serial Monitor.
- Expected Result: System prints debug help text only. No security event is injected into the runtime flow.

### Case 6.2: Serial Debug Status

- Condition: User types `status` in Serial Monitor.
- Expected Result: System prints local reed state (`door_open`, `window_open`) only. No security event is injected.

## Category 2: Away Mode Intrusions

### Case 2.1: Authorized Entry Attempt

- Condition: System is in `Away` mode and the door opens without a recent vibration spike.
- Expected Result: System starts a `30s` entry deadline, sets level to `Warn`, activates warning buzzer, and publishes `warn_entry`.

### Case 2.2: Entry Timeout

- Condition: Entry deadline expires without valid keypad disarm.
- Expected Result: System escalates to `Alert`, activates alarm buzzer, and publishes `alert_timeout`.

### Case 2.3: Forced Entry by Vibration

- Condition: System is in `Away` mode and `vib_spike` happens shortly before `door_open`.
- Expected Result: Grace period is skipped, level becomes `Alert`, and the system publishes `alert_forced_entry`.

### Case 2.4: Door Opened While Locked

- Condition: System is in `Away` mode and the door opens while the lock state is still locked.
- Expected Result: System escalates immediately to `Alert` and publishes `alert_door`.

### Case 2.5: Window Breach

- Condition: System is in `Away` mode and `window_open` is detected.
- Expected Result: System escalates immediately to `Alert` and publishes `alert_high`.

### Case 2.6: Indoor Motion Step-Up

- Condition: System is in `Away` mode and receives `motion 1`, `motion 2`, `motion 3`, `chk2`, or `chk3`.
- Expected Result: Alert level steps up (`Off -> Warn -> Alert`) and publishes `step_up_alert`.

## Category 3: Night Mode Security

### Case 3.1: Perimeter Breach at Night

- Condition: System is in `Night` mode and detects `door_open`, `window_open`, `vib_spike`, or `motion 3`.
- Expected Result: System escalates immediately to `Alert` and publishes `alert_night_breach`.

### Case 3.2: Indoor Activity Ignored

- Condition: System is in `Night` mode and receives `motion 1`, `motion 2`, `chk1`, `chk2`, or `chk3`.
- Expected Result: System ignores the event and keeps the current alarm level.

## Category 4: Keypad & Local Physical Control

### Case 4.1: Correct PIN Entry

- Condition: User enters the correct code and presses `#`.
- Expected Result: System switches to `Disarm`, clears `is_night`, resets failed attempts, unlocks the door, starts a door session, and publishes `mode_disarm`.

### Case 4.2: Wrong PIN Warning / Alert

- Condition: User enters a wrong code and presses `#`.
- Expected Result: `failed_attempts` increments. Attempt `1-2` publishes `wrong_code` with warning buzzer. Attempt `3+` publishes `keypad_alert` with alert buzzer.

### Case 4.3: Keypad Help Request

- Condition: User presses keypad button `B`.
- Expected Result: System publishes `keypad_help` for external notification routing.

### Case 4.4: Keypad Silence

- Condition: User presses keypad button `A`.
- Expected Result: Warning buzzer is silenced. If a door-hold warning is active, the hold warning is marked as silenced for the current door session.

### Case 4.5: Backspace / Clear

- Condition: User presses `*` or `C`.
- Expected Result: `*` removes the last PIN character. `C` clears the whole buffer. Sensor polling is skipped for that tick.

### Case 4.6: Local Door Lock/Unlock Toggle Button

- Condition: User presses the local physical door lock/unlock toggle button on `GPIO33`.
- Expected Result:
  - If the door is locked: unlock the door, start a new door session, publish event `manual_door_toggle`, and publish status reason `manual_door_unlock`.
  - If the door is unlocked: lock the door, clear the door session, publish event `manual_door_toggle`, and publish status reason `manual_door_lock`.

### Case 4.7: Local Window Lock/Unlock Toggle Button

- Condition: User presses the local physical window lock/unlock toggle button on `GPIO18`.
- Expected Result:
  - If the window is locked: unlock the window and publish status reason `manual_window_unlock`.
  - If the window is unlocked: lock the window and publish status reason `manual_window_lock`.

## Category 5: Remote Control & Connectivity

### Case 5.1: Remote Arm Night

- Condition: MQTT command is `arm night` / `arm_night` and `latest_mode == Disarm`.
- Expected Result: System switches to `Night`, saves `is_night = true` to NVS, keeps the base mode in `latest_mode`, and publishes `mode_night`.

### Case 5.2: Remote Arm Night Rejected

- Condition: MQTT command is `arm night` / `arm_night` but `latest_mode != Disarm`.
- Expected Result: No mode change. Bridge receives ack detail `allowed_only_when_latest_disarm`.

### Case 5.3: Remote Night Off

- Condition: MQTT command is `night_off` while current mode is `Night`.
- Expected Result: System reverts to the saved base mode (`Disarm` or `Away`), clears `is_night`, saves to NVS, and publishes the resulting mode.

### Case 5.4: Remote Utility Commands

- Condition: MQTT command is `lock door`, `unlock door`, `lock window`, `unlock window`, `lock all`, `unlock all`, `silence`, or `status`.
- Expected Result:
  - Lock/unlock commands actuate hardware immediately and publish remote status reasons.
  - `silence` stops the buzzer and publishes `remote_silence`.
  - `status` queues a status snapshot with reason `remote_status`.

### Case 5.5: Unsupported Remote Command

- Condition: MQTT command is unsupported, including `disarm`, `arm_away`, or arbitrary unknown text.
- Expected Result: No state change. The system only publishes a failed ack with detail `unsupported`.

### Case 5.6: Network Disconnect Handling

- Condition: Wi-Fi or MQTT broker becomes unavailable.
- Expected Result: `MqttTask` keeps retrying reconnect in the background while `SecTask` continues keypad/manual-button/sensor polling without blocking the main 20 ms loop.
