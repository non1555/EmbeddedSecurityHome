# Possible Cases & Logic Conditions

เอกสารนี้สรุปเคสการทำงานที่สำคัญของระบบ โดยยึดพฤติกรรมจริงจาก `README.md`, `docs/flowchart.mmd`, `docs/layer_structure.md` และโค้ดใน `src/main_board/*`

ใช้หลักตีความดังนี้:
- `Event` คือเหตุการณ์ที่เข้าสู่ `RuleEngine`
- `Status reason` คือเหตุผลที่ถูก publish ไปยัง MQTT status topic
- `Flag` คือ token หลักที่ใช้บอกผลลัพธ์ของ decision/event ในระบบ

## Category 1: Keypad & Local Physical Control

### Case 1.1: Correct PIN Entry

- Condition: ผู้ใช้กรอก PIN ถูกต้องและกด `#`
- Expected Result:
  - ระบบเปลี่ยนเป็น `Disarm`
  - รีเซ็ตจำนวนครั้งที่กรอกรหัสผิด
  - ประตูถูกปลดล็อกและเริ่ม session ใหม่
  - มีการส่งสถานะ `mode_disarm`

### Case 1.2: Wrong PIN Warning / Alert

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

### Case 1.3: Keypad Help Request

- Condition: ผู้ใช้กดปุ่ม `B` บน keypad
- Expected Result:
  - ระบบยกระดับเป็น `Alert`
  - เสียงเตือนเป็น alarm ทันที
  - มีการส่งสถานะ `keypad_help`
  - ฝั่งแจ้งเตือนสามารถแปลผลเป็นการส่งคำขอความช่วยเหลือ

### Case 1.4: Keypad Silence

- Condition: ผู้ใช้กดปุ่ม `A` บน keypad
- Expected Result:
  - ระบบหยุดเสียงเตือนแบบ warning ของ session ปัจจุบัน
  - ถ้ามี door-hold warning อยู่ จะไม่เตือนซ้ำใน session เดิม

### Case 1.5: Backspace / Clear

- Condition: ผู้ใช้กด `*` หรือ `C`
- Expected Result:
  - `*` ลบตัวอักษรล่าสุดใน PIN buffer
  - `C` ล้าง PIN buffer ทั้งหมด
  - รอบนั้นของระบบจะจบที่ keypad edit ก่อน ไม่ตรวจ sensor ต่อใน tick เดียวกัน

### Case 1.6: Local Door Lock/Unlock Toggle Button

- Condition: ผู้ใช้กด local physical door lock/unlock toggle button บน `GPIO33`
- Expected Result:
  - ถ้า door ถูก lock อยู่:
    - ระบบปลดล็อกประตู
    - เริ่ม door session ใหม่
    - มีการส่งสถานะ `manual_door_unlock`
  - ถ้า door ถูก unlock อยู่:
    - ระบบล็อกประตู
    - จบ door session
    - มีการส่งสถานะ `manual_door_lock`

### Case 1.7: Local Window Lock/Unlock Toggle Button

- Condition: ผู้ใช้กด local physical window lock/unlock toggle button บน `GPIO18`
- Expected Result:
  - ถ้า window ถูก lock อยู่:
    - ระบบปลดล็อกหน้าต่าง
    - มีการส่งสถานะ `manual_window_unlock`
  - ถ้า window ถูก unlock อยู่:
    - ระบบล็อกหน้าต่าง
    - มีการส่งสถานะ `manual_window_lock`

## Category 2: Remote Control & Connectivity

### Case 2.1: Remote Utility Commands

- Condition: MQTT command เป็น `lock door`, `unlock door`, `lock window`, `unlock window`, `lock all`, `unlock all`, `silence`, หรือ `status`
- Expected Result:
  - คำสั่ง `lock/unlock` ทำงานกับ actuator ทันที
  - คำสั่ง `silence` หยุด buzzer
  - คำสั่ง `status` ส่ง snapshot สถานะล่าสุดกลับ
  - ทุกคำสั่งที่รองรับจะมีผลตอบกลับกลับไปยังฝั่งผู้สั่ง

### Case 2.2: Remote Arm Night

- Condition: MQTT command เป็น `arm night`, `arm_night`, หรือ `mode night` และ `latest_mode == Disarm`
- Expected Result:
  - ระบบเปลี่ยนจากโหมดปัจจุบันเข้าสู่ `Night`
  - ระบบจดจำ base mode เดิมไว้สำหรับการออกจาก `Night`
  - มีการส่งสถานะ `mode_night`

### Case 2.3: Remote Arm Night Rejected

- Condition: MQTT command เป็น `arm night` / `arm_night` แต่ `latest_mode != Disarm`
- Expected Result:
  - ระบบไม่เปลี่ยนโหมด
  - ผู้สั่งงานได้รับผลลัพธ์ว่าไม่อนุญาตในสถานะปัจจุบัน

### Case 2.4: Remote Night Off

- Condition: MQTT command เป็น `night_off` หรือ `night off` ขณะ mode ปัจจุบันเป็น `Night`
- Expected Result:
  - ระบบออกจาก `Night`
  - ระบบกลับไปยัง base mode เดิมที่เคยบันทึกไว้
  - มีการส่งสถานะของโหมดที่กลับไปใช้งานจริง

### Case 2.5: Network Disconnect Handling

- Condition: Wi-Fi หรือ MQTT broker หลุด
- Expected Result:
  - `MqttTask` รับหน้าที่ reconnect ใน background
  - `SecTask` ยังทำงานด้าน security ต่อได้ตามรอบ `20 ms`
  - การ reconnect ไม่ block security loop หลัก

## Category 3: Mode Transitions & Automation

### Case 3.1: Auto-Arm Success

- Condition: ระบบอยู่ใน `Disarm` แล้วเกิดลำดับ `chk1 -> door_open -> door_close` และไม่มี indoor/window activity ภายในช่วง auto-arm exit window
- Expected Result:
  - ระบบเปลี่ยนโหมดจาก `Disarm` เป็น `Away`
  - ประตูและหน้าต่างถูกสั่งล็อกตามโหมดใหม่
  - มีการส่งสถานะ `mode_away`

### Case 3.2: Auto-Arm Cancelled

- Condition: ระบบอยู่ใน `Disarm`, เข้า `exit_stage == 3` แล้วตรวจพบ `motion 1`, `motion 2`, `motion 3`, `chk2`, หรือ `chk3`
- Expected Result:
  - ระบบยกเลิกการ auto-arm
  - ระบบยังคงอยู่ใน `Disarm`
  - มีการส่งผลลัพธ์ `auto_arm_cancel`

### Case 3.3: Auto-Arm Stage Timeout Reset

- Condition: ระบบเข้า auto-arm stage `1` หรือ `2` แต่ไม่เกิด step ถัดไปภายใน `15s`
- Expected Result:
  - ระบบล้างลำดับ auto-arm ที่ค้างอยู่
  - ระบบไม่ arm ต่อจาก sequence เก่า
  - มีการส่งสถานะ `auto_arm_timeout_reset`

### Case 3.4: Door Auto-Lock After Close

- Condition: ประตูถูก unlock ด้วย PIN, remote command, หรือ local door toggle จากนั้นประตูถูกปิดและค้างปิดครบ `3s`
- Expected Result:
  - ระบบล็อกประตูอัตโนมัติ
  - session ของประตูสิ้นสุดลง
  - มีการส่งสถานะ `auto_locked`

### Case 3.5: Door Unlock Timeout

- Condition: ประตูถูก unlock แต่ไม่เคยถูกเปิด และครบเวลา unlock session `15s`
- Expected Result:
  - ระบบล็อกประตูอัตโนมัติ
  - session ของประตูสิ้นสุดลง
  - มีการส่งสถานะ `auto_locked_timeout`

### Case 3.6: Power Loss Recovery

- Condition: ESP32 restart หลังไฟดับหรือ reset
- Expected Result:
  - ระบบกลับมาทำงานด้วยสถานะล่าสุดที่บันทึกไว้
  - โหมด `Night` จะถูกคืนค่าถ้ายังเปิดอยู่ก่อนหน้า
  - มีการส่งสถานะ `boot`

## Category 4: Away Mode Intrusions

### Case 4.1: Outside Motion Warning

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `motion 3`
- Expected Result:
  - ระบบยกระดับเป็น `Warn`
  - เสียงเตือนเป็น warning
  - มีการส่งสถานะ `warn_outside_motion`

### Case 4.2: Door Breach in Away

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `door_open`
- Expected Result:
  - หากไม่มีเงื่อนไข forced entry จะถือเป็นการบุกรุกผ่านประตูทั่วไป
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_door`

### Case 4.3: Forced Entry by Vibration / Tamper

- Condition: ระบบอยู่ใน `Away` และ `door_open` เกิดภายใน forced-entry window หลัง `vib_spike` หรือมี `door_tamper`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_forced_entry`

### Case 4.4: Window Breach

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `window_open`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_high`

### Case 4.5: High-Risk Intrusion Triggers

- Condition: ระบบอยู่ใน `Away` และตรวจพบ `vib_spike`, `motion 1`, `motion 2`, `chk2`, หรือ `chk3`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_high`

## Category 5: Night Mode Security

### Case 5.1: Indoor Activity Ignored

- Condition: ระบบอยู่ใน `Night` และตรวจพบ `motion 1`, `motion 2`, `chk1`, `chk2`, หรือ `chk3`
- Expected Result:
  - ระบบไม่เปลี่ยนระดับการเตือน
  - ไม่สร้าง alarm ใหม่จากเหตุการณ์กลุ่มนี้
  - ระบบคงโหมด `Night` ต่อไป

### Case 5.2: Perimeter Breach at Night

- Condition: ระบบอยู่ใน `Night` และตรวจพบ `door_open`, `window_open`, `vib_spike`, หรือ `motion 3`
- Expected Result:
  - ระบบยกระดับเป็น `Alert` ทันที
  - เสียงเตือนเปลี่ยนเป็น alarm
  - มีการส่งสถานะ `alert_night_breach`
