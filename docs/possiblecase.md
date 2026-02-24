2. Possible Cases & Logic Conditions
Category 1: Mode Transitions & Automation

Case 1.1: Auto-Arm Success


Condition: System is in Disarm mode --> Chokepoint 1 triggered --> Door opened --> Door closed --> No indoor motion detected for 20 seconds.


Expected Result: System automatically switches to Away mode --> Saves state to NVS (updating latest_mode = Away) --> Publishes MQTT message mode_away.

Case 1.2: Auto-Arm Cancelled


Condition: System is counting down 20 seconds for Auto-Arm --> Indoor motion sensor (mot1, mot2, mot3, chk2, chk3) is triggered.


Expected Result: Countdown is aborted --> System reverts to Disarm mode to prevent locking occupants inside.

Case 1.3: Door Auto-Lock


Condition: Door status remains closed (door_open is LOW) for exactly 3 seconds.


Expected Result: Triggers servo1.lock() --> Publishes MQTT message auto_locked.

Case 1.4: Power Loss & NVS Recovery (State Override Logic)


Condition: ESP32 board loses power and restarts.


Expected Result: System loads the last saved state from NVS memory during setup(). Specifically, it evaluates the is_night flag. If true, it activates Night mode while retaining latest_mode in RAM. If false, it directly resumes the base latest_mode (Disarm or Away).

Category 2: Away Mode Intrusions (Threat Detection)

Case 2.1: Grace Period Entry (Authorized Entry Attempt)


Condition: System is in Away mode --> Door is opened (door_open) WITHOUT any prior vibration spike.


Expected Result: Initiates a 30-second Grace Period --> Escalates level to Warn --> Publishes MQTT message warn_entry.

Case 2.2: Entry Timeout


Condition: Grace Period exceeds 30 seconds without a valid PIN entry at the Keypad.


Expected Result: Escalates level to Alert immediately --> Triggers Siren --> Publishes MQTT message alert_timeout.

Case 2.3: Forced Entry (Door Breached)


Condition: System is in Away mode --> Vibration spike (vib_spike) is detected simultaneously with or just before the door opens.


Expected Result: Bypasses Grace Period --> Escalates level to Alert immediately --> Publishes MQTT message alert_forced_entry.

Case 2.4: Window Breach


Condition: System is in Away mode --> Window magnetic sensor (window_open) is triggered.


Expected Result: Escalates level to Alert immediately --> Publishes MQTT message alert_high.

Case 2.5: Sneak-in Detection (StepUp Escalation)


Condition: System is in Away mode --> Indoor motion sensor (mot1, mot2, mot3, or ultrasonics) is triggered.


Expected Result: Triggers StepUp escalation logic (Off --> Warn --> Alert based on continuous movement) --> Publishes MQTT message step_up_alert.

Category 3: Night Mode Security (Arm Stay)

Case 3.1: Perimeter Breach


Condition: System is in Night mode --> Any perimeter sensor (door_open, window_open, vib_spike, or window PIR mot3) is triggered.


Expected Result: Bypasses any warnings --> Escalates level to Alert immediately --> Triggers Siren --> Publishes MQTT message alert_night_breach.

Case 3.2: Sleep Safe Ignore (Indoor Movement)


Condition: System is in Night mode --> Any indoor sensor (mot1, mot2, chk1, chk2, chk3) is triggered by an occupant.


Expected Result: System ignores the event --> No warnings or alerts are triggered --> Siren remains silent.

Category 4: Keypad & Access Control

Case 4.1: Correct PIN Entry (Night Flag Override)


Condition: User inputs correct PIN --> Presses # (Confirm).


Expected Result: System switches to Disarm mode --> Forces is_night = false and latest_mode = Disarm in NVS --> Unlocks door (servo1.unlock()) --> Resets failed_attempts counter to 0 --> Publishes MQTT message mode_disarm.

Case 4.2: Wrong PIN Warning


Condition: User inputs incorrect PIN --> Presses # (1st or 2nd attempt).


Expected Result: Increments failed_attempts counter --> Escalates level to Warn --> Triggers short buzzer beep --> Publishes MQTT message wrong_code.

Case 4.3: Anti-Brute Force Alert (3 Strikes)


Condition: The failed_attempts counter reaches 3.


Expected Result: Escalates level to Alert immediately --> Triggers Siren --> Publishes MQTT message keypad_alert.

Case 4.4: Panic Button (SOS)


Condition: User presses the B key on the Keypad.


Expected Result: Generates a help event --> Publishes MQTT message keypad_help immediately for external notification routing.

Case 4.5: Silence Warning Mute


Condition: System is in Warn state (beeping) --> User presses the A key.


Expected Result: Executes buzzer.stop() to silence the warning tone while the user inputs the PIN.

Case 4.6: Keypad Backspace


Condition: User makes a typo and presses the * key.


Expected Result: Removes the last entered character from the PIN buffer and updates the OLED display accordingly.

Case 4.7: Keypad Clear Buffer


Condition: User presses the C key.


Expected Result: Clears the entire PIN buffer completely, resets the input state, and updates the OLED display.

Category 5: Remote Control & Connectivity (Strict Security Mode)

Case 5.1: Remote Arm Night (Strict Override)


Condition: Receives MQTT payload arm_night from a remote client AND latest_mode == Disarm.


Expected Result: System switches to Night mode --> Saves state to NVS (Setting is_night = true while preserving latest_mode) --> Publishes MQTT message mode_night. Note: If latest_mode is Away, this command is strictly ignored.

Case 5.2: Remote Night Off (User Error Revert)

Condition: Receives MQTT payload night_off AND the system is currently in Night mode.

Expected Result: System safely reverts back to latest_mode --> Sets is_night = false in NVS --> Publishes the updated mode to MQTT.

Case 5.3: Remote Hardware Override & Utilities


Condition: Receives MQTT payload lock door, unlock door, lock window, unlock window, lock all, unlock all, or silence.


Expected Result: Executes the specific hardware command immediately, regardless of the active mode. The silence command mutes the buzzer without altering the system's Alert level.

Case 5.4: Remote Status Request

Condition: Receives MQTT payload status.

Expected Result: System enqueues a status publish request immediately, and SecTask calls MQTT loop/tick to flush a non-blocking status payload containing current mode, alert level, and event drop counter.

Case 5.6: Network Disconnection Handling


Condition: The mqttBus_.isConnected() function returns False.


Expected Result: System executes a non-blocking asynchronous reconnect sequence within the main SecTask loop while sensor/keypad polling continues seamlessly without freezing or watchdog resets.
