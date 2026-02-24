# EmbeddedSecurityHome

An ESP32-based smart home security system running FreeRTOS, integrated with LINE OA for real-time notifications and remote control via MQTT.

This document serves as the Single Source of Truth for:
- Understanding the system scope and logic.
- End-to-end installation and deployment.
- Pre-delivery architectural validation.

---

## 1. Core Features

- **Security Modes:** Supports 3 primary modes: `Disarm`, `Away`, and `Night`.
- **Intrusion Detection:** Monitors events via a sensor array:
  - Reed Switches (Doors / Windows)
  - PIR Motion Sensors
  - Vibration Sensor (Tamper / Forced entry)
  - Ultrasonic Sensors (Round-robin + Timeout execution)
- **Progressive Alert:** Escalating threat logic (`Off -> Warn -> Alert`).
- **Access Control:** 4x4 Keypad PIN flow (`*` = Backspace, `C` = Clear buffer).
- **Remote Control (LINE/MQTT):**
  - Lock/Unlock doors and windows.
  - Mode shifting (`arm_night`, `night_off`).
  - Temporary buzzer `silence` and system `status` requests.
- **State Persistence:** Saves critical states (`latest_mode`, `is_night`) to NVS memory for power-loss recovery.

## 2. System Diagrams

### Architecture
![System Block Diagram](docs/blockdiagram.svg)

### Runtime Flow
![Main Flowchart](docs/flowchart.svg)

### Wiring References
- ![Pin Allocation Table](docs/pinallocation_table.svg)
- ![Pin Allocate Board](docs/pinallocate_board.svg)

## 3. Runtime Contract

- **RTOS Execution:** Single `SecTask` loop with a `20 ms` period budget.
- **Event Management:** Handled via `eventQueue`, `commandQueue`, and `publishQueue`.
- **Network Task:** Non-blocking MQTT tick executed within `SecTask`.
- **Remote Security Policy:**
  - `arm_night` is only permitted when `latest_mode == Disarm`.
  - `night_off` is only permitted when currently in `Night` mode (reverts to the previous `latest_mode`).

---

## 4. Setup & Installation (Windows, UI-First)

Primary path for this project is the **UI Launcher**.  
Use CMD/Terminal only as a secondary fallback for troubleshooting or automation.

### 4.1 Prerequisites
- Python 3.10+
- Git
- MQTT Broker (Mosquitto recommended)
- ESP32 USB Drivers (CP210x / CH340)
- LINE Developers Account (Messaging API)
- ngrok Account + Authtoken

### 4.2 Get ngrok Authtoken (One-time)
1. Sign up / sign in at [ngrok dashboard](https://dashboard.ngrok.com/).
2. Open **Your Authtoken** page: [https://dashboard.ngrok.com/get-started/your-authtoken](https://dashboard.ngrok.com/get-started/your-authtoken).
3. Click **Copy** to copy your token.
4. Paste token into Launcher **Config** tab field `NGROK_AUTHTOKEN`, then **Save to .env**.
5. (Optional, machine-level) Run once in terminal:
```bat
ngrok config add-authtoken <YOUR_NGROK_AUTHTOKEN>
```

### 4.3 First-time Setup
Open Command Prompt at the project root and run once:
```bat
setup.cmd
```

### 4.4 Launch Control Center (Primary Path)
Open the system management UI:
```bat
tools\line_bridge\start-ui.cmd
```
*(Alternatively, double-click `tools/line_bridge/launcher.vbs`)*

### 4.5 System Configuration
In the Launcher window:
1. **Config Tab:**
   - Enter your `LINE_CHANNEL_ACCESS_TOKEN` and `LINE_CHANNEL_SECRET`.
   - Enter your `NGROK_AUTHTOKEN`.
2. **Firmware Tab:**
   - Enter Wi-Fi credentials (SSID / Password).
   - Enter MQTT Broker details (IP / Port / Username / Password).
   - Set the `FW_DOOR_CODE` (4-digit Keypad PIN).
   - Click **Save to .env**.

### 4.6 Start Bridge + Webhook (from UI)
1. Go to the **Status** tab.
2. Click **Start**.
3. Wait until ngrok URL appears, then click **Copy Webhook**.
4. Use that URL in LINE Developers (steps in Section 5).

### 4.7 Deploy Firmware (from UI)
1. Connect the ESP32 board via USB.
2. In the Firmware tab, click **Refresh Ports** and select the correct COM port.
3. Click **Deploy Main Board** to compile and flash the firmware.

### 4.8 Secondary Path (CMD/Terminal Fallback)
Use this only when UI cannot be used.

Firmware build/upload:
```powershell
pio run -e main-board
pio run -e main-board -t upload
pio device monitor -b 115200
```

Bridge runtime (manual):
```bat
cd tools\line_bridge
run.cmd
```

---

## 5. Webhook Integration (LINE OA)

### 5.1 Recommended: Configure via UI
1. In Launcher **Status** tab, click **Start**.
2. Wait for ngrok URL, then click **Copy Webhook**.
3. Go to [LINE Developers Console](https://developers.line.biz/) > your channel > **Messaging API**.
4. Paste URL ending with `/line/webhook`.
5. Click **Verify** and enable **Use webhook**.

### 5.2 Secondary: Manual (outside UI)
1. Set ngrok token once on your machine:
```bat
ngrok config add-authtoken <YOUR_NGROK_AUTHTOKEN>
```
2. Ensure `tools\line_bridge\.env` has:
   - `LINE_CHANNEL_ACCESS_TOKEN`
   - `LINE_CHANNEL_SECRET`
   - `HTTP_PORT=8080` (or your chosen port)
3. Start bridge + tunnel:
```bat
cd tools\line_bridge
run.cmd
```
4. Open ngrok inspector `http://127.0.0.1:4040`, copy HTTPS URL, then append `/line/webhook`.
5. In LINE Developers > **Messaging API**:
   - Paste webhook URL
   - Press **Verify**
   - Turn on **Use webhook**

---

## 6. Command Reference

| Command | Effect |
|---|---|
| `lock door` / `unlock door` | Actuate the main door lock |
| `lock window` / `unlock window` | Actuate the window lock |
| `lock all` / `unlock all` | Actuate all locks simultaneously |
| `arm_night` | Enter Night mode (perimeter monitoring only) |
| `night_off` | Exit Night mode and revert to the previous mode |
| `silence` | Temporarily mute the buzzer (alert state remains active) |
| `status` | Request a snapshot of the current system state |

## 7. MQTT Topics (Contract)

- `esh/main/cmd` (ESP32 Subscribe - Receives commands)
- `esh/main/event` (ESP32 Publish - Emits hardware/security events)
- `esh/main/status` (ESP32 Publish - Emits state snapshots)
- `esh/main/ack` (ESP32 Publish - Acknowledges command execution)
- `esh/main/metrics` (ESP32 Publish - Emits system health/performance data)

## 8. Project Structure

- `src/main_board/*` : Source code for the primary ESP32 firmware.
- `tools/line_bridge/*` : Source code for the LINE Bridge and UI Launcher.
- `docs/*` : Documentation hub (Block Diagram, Flowchart, Pin Allocation).

---

## 9. Possible Cases & Expected Behaviors

This section defines the expected system behavior under various scenarios, strictly reflecting the current codebase implementation and RTOS priority polling architecture.

### Category 1: Disarm Mode & Auto-Arm Sequence
* **Case 1.1: Successful Auto-Arming**
  * **Condition:** System is in `Disarm` mode. `chk1` (Ultrasonic 1) is triggered -> `door_open` -> `door_closed` -> No indoor/window motion for 20 seconds.
  * **Expected Result:** The system sequentially shifts `exit_stage` from 1 to 3. Upon 20s timeout, the system automatically transitions to `Away` mode, saves the state to NVS, and publishes `mode_away` via MQTT.
* **Case 1.2: Auto-Arming Cancellation (Activity Detected)**
  * **Condition:** System is in the 20-second exit countdown (`exit_stage == 3`). An indoor PIR or window sensor detects motion.
  * **Expected Result:** The auto-arm sequence is aborted. `exit_stage` resets to 0. The system remains in `Disarm` mode.

### Category 2: Away Mode & Intrusion Defense
* **Case 2.1: Authorized Entry (Grace Period)**
  * **Condition:** System is in `Away` mode. The main door is opened (`door_open`) WITHOUT prior vibration spikes.
  * **Expected Result:** The system enters a 30-second Grace Period. Alert level becomes `Warn`. The buzzer beeps as a warning, and `warn_entry` is published to MQTT.
* **Case 2.2: Grace Period Timeout**
  * **Condition:** The 30-second Grace Period expires without a valid PIN entry from the keypad.
  * **Expected Result:** System escalates to `Alert` level. The buzzer sounds a continuous alarm, and `alert_timeout` is published.
* **Case 2.3: Forced Entry (Vibration + Door Open)**
  * **Condition:** System is in `Away` mode. A vibration spike (`vib_spike`) is detected, followed closely by a `door_open` event.
  * **Expected Result:** The Grace Period is bypassed entirely. The system instantly escalates to `Alert` level, triggering the siren and publishing `alert_forced_entry`.
* **Case 2.4: Window Breach**
  * **Condition:** System is in `Away` mode. A window is opened (`window_open`).
  * **Expected Result:** Instant escalation to `Alert` level. Publishes `alert_high`.
* **Case 2.5: Indoor Motion (Step-Up Alert)**
  * **Condition:** System is in `Away` mode (but currently `Off` or `Warn` level). Indoor PIR detects motion.
  * **Expected Result:** The alert level steps up (e.g., from `Off` to `Warn`, or `Warn` to `Alert`). Publishes `step_up_alert`.

### Category 3: Night Mode & Perimeter Defense
* **Case 3.1: Perimeter Breach**
  * **Condition:** System is in `Night` mode. `door_open`, `window_open`, `vib_spike`, or `motion 3` (Window PIR) is triggered.
  * **Expected Result:** Instant escalation to `Alert` level. Publishes `alert_night_breach`.
* **Case 3.2: Indoor Activity (Ignored)**
  * **Condition:** System is in `Night` mode. An indoor PIR (`motion 1` or `motion 2`) is triggered by a resident.
  * **Expected Result:** The system intentionally ignores the event. No alert is triggered, allowing safe movement inside the house.

### Category 4: Keypad & Access Control
* **Case 4.1: Valid PIN Entry**
  * **Condition:** User inputs the correct 4-digit PIN and presses `#`.
  * **Expected Result:** System immediately switches to `Disarm` mode, resets failed attempts to 0, unlocks the main door, and clears the `is_night` flag in NVS.
* **Case 4.2: Invalid PIN Entry & Anti-Brute Force**
  * **Condition:** User inputs an incorrect PIN and presses `#`.
  * **Expected Result:** `failed_attempts` increments. If attempts < 3, level is `Warn` (`wrong_code`). If attempts >= 3, level becomes `Alert` (`keypad_alert`).
* **Case 4.3: Hardware Backspace & Clear**
  * **Condition:** User makes a typo and presses `*` (Backspace) or `C` (Clear).
  * **Expected Result:** `*` removes the last character from the buffer. `C` empties the entire buffer.

### Category 5: Remote Control & Network (Strict Policy)
* **Case 5.1: Remote Arm Night**
  * **Condition:** Receives `arm_night` payload via MQTT.
  * **Expected Result:**
    * If current `latest_mode` == `Disarm`: Switches to `Night` mode, sets `is_night=true` in NVS, and retains the previous mode memory.
    * If current `latest_mode` != `Disarm` (e.g., Away): Command is ignored.
* **Case 5.2: Remote Night Off**
  * **Condition:** Receives `night_off` payload via MQTT.
  * **Expected Result:** If currently in `Night` mode, system safely reverts to `latest_mode` (e.g., back to Disarm) and updates NVS.
* **Case 5.3: Security Policy Violation (Forbidden Commands)**
  * **Condition:** Receives `disarm` or `arm_away` payload via MQTT.
  * **Expected Result:** The payload is instantly dropped by the parser. No action is taken. (Prevents remote network hacking to unlock the house).
* **Case 5.4: Utility Commands**
  * **Condition:** Receives `lock door`, `unlock door`, `silence`, or `status`.
  * **Expected Result:** Command is queued and executed unconditionally regardless of the current mode.

### Category 6: RTOS & Priority Polling Integrity
* **Case 6.1: Simultaneous Sensor Triggers**
  * **Condition:** Multiple sensors (e.g., `door_open` and `motion 1`) trigger at the exact same millisecond.
  * **Expected Result:** Due to the 1-Event/Tick Priority Polling architecture, `EventCollector.poll()` captures `door_open` first and performs an Early Exit. The `motion 1` event is captured in the *next* 20ms RTOS tick. This prevents CPU budget overflow and guarantees deterministic RTOS behavior.
* **Case 6.2: Network Disconnect**
  * **Condition:** Wi-Fi or MQTT broker goes offline.
  * **Expected Result:** `context.isMqttConnected()` returns false. The `SecTask` bypasses the MQTT read block and continues polling the keypad and sensors seamlessly. The system remains 100% operational locally.

---


