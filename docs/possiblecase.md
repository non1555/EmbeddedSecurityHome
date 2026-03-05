# Possible Cases & Logic Conditions

เอกสารนี้สรุปเคสการทำงานที่สำคัญของระบบ โดยยึดพฤติกรรมจริงจาก `README.md`, `docs/flowchart.mmd`, `docs/layer_structure.md` และโค้ดใน `src/main_board/*`

ใช้หลักตีความดังนี้:
- `Event` คือเหตุการณ์ที่เข้าสู่ `RuleEngine`
- `Status reason` คือเหตุผลที่ถูก publish ไปยัง MQTT status topic
- `Flag` คือ token หลักที่ใช้บอกผลลัพธ์ของ decision/event ในระบบ

## Category 1: Global Alert Overrides

### Case 1.1: Vibration Alert in All Modes

- Condition: ตรวจพบ `vib_spike` ในโหมดใดก็ตาม
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันทีทุกโหมด
  - เสียงเตือนเปลี่ยนเป็น alarm
  - ถ้าอยู่ `Night` จะใช้สถานะ `alert_night_breach`
  - ถ้าอยู่โหมดอื่นจะใช้สถานะ `alert_high`

### Case 1.2: Locked Opening Alert in All Modes

- Condition: ตรวจพบ `door_open` ขณะ `door_locked == true` หรือ `window_open` ขณะ `window_locked == true`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันทีทุกโหมด
  - เสียงเตือนเปลี่ยนเป็น alarm
  - ประตูใช้สถานะ `alert_door`
  - หน้าต่างใช้สถานะ `alert_high` หรือ `alert_night_breach` ตามโหมด

## Category 2: Keypad & Local Physical Control

### Case 2.1: Correct PIN Entry

- Condition: ผู้ใช้กรอก PIN ถูกต้องและกด `#`
- Expected Result:
  - ระบบเปลี่ยนเป็น `Disarm`
  - รีเซ็ตจำนวนครั้งที่กรอกรหัสผิด
  - ประตูถูกปลดล็อกและเริ่ม door session ใหม่
  - มีการส่งสถานะ `mode_disarm`

### Case 2.2: Wrong PIN Warning / Alert

- Condition: ผู้ใช้กรอก PIN ผิดและกด `#`
- Expected Result:
  - `failed_attempts` เพิ่มขึ้น
  - ถ้าผิดครั้งที่ `1-2`:
    - ระบบยกระดับเป็น `Warn`
    - เสียงเตือนเป็น warning
    - มีการส่งสถานะ `wrong_code`
  - ถ้าผิดตั้งแต่ครั้งที่ `3` ขึ้นไป:
    - ระบบยกระดับเป็น `Alert`
    - เสียงเตือนเป็น alarm
    - มีการส่งสถานะ `keypad_alert`

### Case 2.3: Keypad Help Request

- Condition: ผู้ใช้กดปุ่ม `B` บน keypad
- Expected Result:
  - ระบบยกระดับเป็น `Alert`
  - เสียงเตือนเป็น alarm ทันที
  - มีการส่งสถานะ `keypad_help`
  - ฝั่งแจ้งเตือนสามารถแปลผลเป็นการส่งคำขอความช่วยเหลือฉุกเฉิน

### Case 2.4: Keypad Silence

- Condition: ผู้ใช้กดปุ่ม `A` บน keypad
- Expected Result:
  - ระบบหยุดเสียงเตือนแบบ warning ของ door session ปัจจุบัน
  - ถ้ามี door-hold warning อยู่ จะไม่เตือนซ้ำใน session เดิม

### Case 2.5: Backspace / Clear

- Condition: ผู้ใช้กด `*` หรือ `C`
- Expected Result:
  - `*` ลบตัวอักษรล่าสุดใน PIN buffer
  - `C` ล้าง PIN buffer ทั้งหมด
  - รอบนั้นของระบบจะจบที่ keypad edit ก่อน และไม่ตรวจ sensor ต่อใน tick เดียวกัน

### Case 2.6: Local Door Lock/Unlock Toggle Button

- Condition: ผู้ใช้กด local physical door lock/unlock toggle button บน `GPIO33`
- Expected Result:
  - ถ้าประตูถูกล็อกอยู่:
    - ระบบปลดล็อกประตู
    - เริ่ม door session ใหม่
    - มีการส่งสถานะ `manual_door_unlock`
  - ถ้าประตูถูกปลดล็อกอยู่และประตูปิดอยู่:
    - ระบบล็อกประตู
    - จบ door session
    - มีการส่งสถานะ `manual_door_lock`

### Case 2.7: Local Door Lock Rejected While Open

- Condition: ผู้ใช้กดปุ่มประตูขณะประตูยังเปิดอยู่ และสถานะปัจจุบันคือ unlocked
- Expected Result:
  - ระบบไม่สั่งล็อกประตู
  - ระบบยกระดับเป็น `Warn`
  - เสียงเตือนเป็น warning
  - มีการส่งสถานะ `warn_lock_door_open`
  - มีการแจ้งเตือนใน LINE ว่าไม่สามารถล็อกได้เพราะประตูยังเปิดอยู่

### Case 2.8: Local Window Lock/Unlock Toggle Button

- Condition: ผู้ใช้กด local physical window lock/unlock toggle button บน `GPIO18`
- Expected Result:
  - ถ้าหน้าต่างถูกล็อกอยู่:
    - ระบบปลดล็อกหน้าต่าง
    - มีการส่งสถานะ `manual_window_unlock`
  - ถ้าหน้าต่างถูกปลดล็อกอยู่และหน้าต่างปิดอยู่:
    - ระบบล็อกหน้าต่าง
    - มีการส่งสถานะ `manual_window_lock`

### Case 2.9: Local Window Lock Rejected While Open

- Condition: ผู้ใช้กดปุ่มหน้าต่างขณะหน้าต่างยังเปิดอยู่ และสถานะปัจจุบันคือ unlocked
- Expected Result:
  - ระบบไม่สั่งล็อกหน้าต่าง
  - ระบบยกระดับเป็น `Warn`
  - เสียงเตือนเป็น warning
  - มีการส่งสถานะ `warn_lock_window_open`
  - มีการแจ้งเตือนใน LINE ว่าไม่สามารถล็อกได้เพราะหน้าต่างยังเปิดอยู่

## Category 3: Remote Control & Connectivity

### Case 3.1: Remote Status / Silence / Unlock Commands

- Condition: MQTT command เป็น `status`, `silence`, `unlock door`, `unlock window`, หรือ `unlock all`
- Expected Result:
  - `status` ส่ง snapshot สถานะล่าสุดกลับ
  - `silence` หยุด buzzer ตามคำสั่ง
  - `unlock door` และ `unlock all` เริ่ม door session ใหม่
  - ทุกคำสั่งที่รองรับจะมี ack กลับไปยังฝั่งผู้สั่ง

### Case 3.2: Remote Lock Commands

- Condition: MQTT command เป็น `lock door`, `lock window`, หรือ `lock all` และอุปกรณ์ที่เกี่ยวข้องปิดอยู่แล้ว
- Expected Result:
  - ระบบสั่งล็อก actuator ที่เกี่ยวข้องทันที
  - ถ้าเป็น `lock door` หรือ `lock all` ระบบจะจบ door session ปัจจุบัน
  - มีการส่ง status reason ของคำสั่งที่สำเร็จกลับไปยังฝั่งแจ้งเตือน

### Case 3.3: Remote Lock Rejected While Open

- Condition: MQTT command เป็น `lock door`, `lock window`, หรือ `lock all` แต่ยังมีประตูหรือหน้าต่างเปิดอยู่
- Expected Result:
  - ระบบไม่สั่งล็อก actuator
  - ระบบยกระดับเป็น `Warn`
  - เสียงเตือนเป็น warning
  - มีการส่ง failed ack พร้อมเหตุผลของจุดที่ยังเปิดอยู่
  - มีการส่งสถานะ `warn_lock_door_open`, `warn_lock_window_open`, หรือ `warn_lock_all_open`

### Case 3.4: Remote Help Request

- Condition: LINE หรือ MQTT ส่งคำสั่ง `keypad_help` หรือ `help`
- Expected Result:
  - ระบบทำงานเหมือนการกดปุ่ม `B` บน keypad
  - ระบบยกระดับเป็น `Alert`
  - เสียงเตือนเป็น alarm ทันที
  - มีการส่งสถานะ `keypad_help`

### Case 3.5: Remote Arm Night

- Condition: MQTT command เป็น `arm night`, `arm_night`, หรือ `mode night` และ `latest_mode == Disarm`
- Expected Result:
  - ระบบเปลี่ยนจากโหมดปัจจุบันเข้าสู่ `Night`
  - ระบบจดจำ base mode เดิมไว้สำหรับการออกจาก `Night`
  - มีการส่งสถานะ `mode_night`

### Case 3.6: Remote Arm Night Rejected

- Condition: MQTT command เป็น `arm night` / `arm_night` แต่ `latest_mode != Disarm`
- Expected Result:
  - ระบบไม่เปลี่ยนโหมด
  - ฝั่งผู้สั่งได้รับผลลัพธ์ว่าไม่อนุญาตในสถานะปัจจุบัน

### Case 3.7: Remote Night Off

- Condition: MQTT command เป็น `night_off` หรือ `night off` ขณะ mode ปัจจุบันเป็น `Night`
- Expected Result:
  - ระบบออกจาก `Night`
  - ระบบกลับไปยัง base mode เดิมที่เคยบันทึกไว้
  - มีการส่งสถานะของโหมดที่กลับไปใช้งานจริง

### Case 3.8: Network Disconnect Handling

- Condition: Wi-Fi หรือ MQTT broker หลุด
- Expected Result:
  - `MqttTask` รับหน้าที่ reconnect ใน background
  - `SecTask` ยังทำงานด้าน security ต่อได้ตามรอบ `20 ms`
  - การ reconnect ไม่ block security loop หลัก

## Category 4: Mode Transitions & Automation

### Case 4.1: Auto-Arm Success

- Condition: ระบบอยู่ใน `Disarm` แล้วเกิดลำดับ `chk1 -> door_open -> door_close` และไม่มี indoor/window activity ภายในช่วง auto-arm exit window
- Expected Result:
  - ระบบเปลี่ยนโหมดจาก `Disarm` เป็น `Away`
  - ประตูและหน้าต่างถูกสั่งล็อกตามโหมดใหม่
  - มีการส่งสถานะ `mode_away`

### Case 4.2: Auto-Arm Cancelled

- Condition: ระบบอยู่ใน `Disarm`, เข้า `exit_stage == 3` แล้วตรวจพบ `motion 1`, `motion 2`, `motion 3`, `chk2`, หรือ `chk3`
- Expected Result:
  - ระบบยกเลิกการ auto-arm
  - ระบบยังคงอยู่ใน `Disarm`
  - มีการส่งผลลัพธ์ `auto_arm_cancel`

### Case 4.3: Auto-Arm Stage Timeout Reset

- Condition: ระบบเข้า auto-arm stage `1` หรือ `2` แต่ไม่เกิด step ถัดไปภายใน `15s`
- Expected Result:
  - ระบบล้างลำดับ auto-arm ที่ค้างอยู่
  - ระบบไม่ arm ต่อจาก sequence เก่า
  - มีการส่งสถานะ `auto_arm_timeout_reset`

### Case 4.4: Door Auto-Lock After Close

- Condition: ประตูถูก unlock ด้วย PIN, remote command, หรือ local door toggle จากนั้นประตูถูกปิดและค้างปิดครบ `3s`
- Expected Result:
  - ระบบสั่งให้ servo ประตูเริ่มล็อกอัตโนมัติ
  - session ของประตูสิ้นสุดลง
  - มีการส่งสถานะ `auto_locked` หลัง servo ถึงตำแหน่งล็อกจริง

### Case 4.5: Door Unlock Timeout

- Condition: ประตูถูก unlock แต่ไม่เคยถูกเปิด และครบเวลา unlock session `15s`
- Expected Result:
  - ระบบสั่งให้ servo ประตูเริ่มล็อกอัตโนมัติ
  - session ของประตูสิ้นสุดลง
  - มีการส่งสถานะ `auto_locked_timeout` หลัง servo ถึงตำแหน่งล็อกจริง

### Case 4.6: Power Loss Recovery

- Condition: ESP32 restart หลังไฟดับหรือ reset
- Expected Result:
  - ระบบกลับมาทำงานด้วยสถานะล่าสุดที่บันทึกไว้
  - โหมด `Night` จะถูกคืนค่าถ้ายังเปิดอยู่ก่อนหน้า
  - มีการส่งสถานะ `boot`

## Category 5: Away Mode Intrusions

### Case 5.1: Outside Motion Warning

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `motion 3`
- Expected Result:
  - ระบบยกระดับเป็น `Warn`
  - เสียงเตือนเป็น warning
  - มีการส่งสถานะ `warn_outside_motion`

### Case 5.2: Door Intrusion in Away

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `door_open`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_door`

### Case 5.3: Other High-Risk Intrusions

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `window_open`, `motion 1`, `motion 2`, `chk2`, หรือ `chk3`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_high`

## Category 6: Night Mode Security

### Case 6.1: Indoor Activity Ignored

- Condition: ระบบอยู่ใน `Night` และตรวจพบ `motion 1`, `motion 2`, `chk1`, `chk2`, หรือ `chk3`
- Expected Result:
  - ระบบไม่เปลี่ยนระดับการเตือน
  - ไม่สร้าง alarm ใหม่จากเหตุการณ์กลุ่มนี้
  - ระบบคงโหมด `Night` ต่อไป

### Case 6.2: Perimeter Breach at Night

- Condition: ระบบอยู่ใน `Night` และตรวจพบ `door_open`, `window_open`, `vib_spike`, หรือ `motion 3`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_night_breach`
