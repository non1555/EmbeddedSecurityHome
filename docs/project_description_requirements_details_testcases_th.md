# คำอธิบายโครงการ (ข้อกำหนด รายละเอียด และกรณีทดสอบ)

สถานะ: ร่างใช้งานจริง (อ้างอิงโค้ดปัจจุบัน)
อัปเดตล่าสุด: 2026-02-19

## 1) ภาพรวมโครงการ

`EmbeddedSecurityHome` คือระบบความปลอดภัยบ้านแบบ Embedded + IoT แบ่งเป็น 3 ส่วนหลัก:
- `Main Board` (ESP32): ตรรกะความปลอดภัยหลัก ตรวจจับการบุกรุก ควบคุมล็อก/สัญญาณเตือน
- `Automation Board` (ESP32): ควบคุมไฟ/พัดลมอัตโนมัติ โดยอิง context จาก Main Board
- `LINE Bridge` (Python): เชื่อม MQTT กับ LINE สำหรับสั่งงานและรับแจ้งเตือน

เป้าหมายหลัก:
- ตรวจจับความเสี่ยงเมื่อระบบอยู่โหมด armed
- แจ้งเตือนผู้ใช้ผ่าน LINE แบบอ่านแล้วเข้าใจทันที
- คำสั่งสำคัญต้องมี token + nonce (กัน replay)
- แยกหน้าที่ security กับ automation ชัดเจน

## 2) ข้อกำหนด (Requirements)

### 2.1 Functional Requirements

| ID | ข้อกำหนด |
|---|---|
| FR-01 | ระบบต้องรองรับโหมด `startup_safe`, `disarm`, `away`, `night` |
| FR-02 | ระบบต้องรับ event จาก reed, PIR, vibration, ultrasonic, keypad, manual buttons |
| FR-03 | ระบบต้องประเมินความเสี่ยงด้วย rule-based scoring และให้ระดับ `off/warn/alert/critical` |
| FR-04 | ระบบต้อง publish MQTT: `event`, `status`, `ack`, `metrics` |
| FR-05 | คำสั่ง remote แบบแก้สถานะต้องใช้ token + nonce (anti-replay) |
| FR-06 | การ lock/unlock ต้องผ่าน policy (mode, sensor-fault fail-closed, contact state) |
| FR-07 | ต้องมี door unlock session: timeout, warning, auto-relock |
| FR-08 | Automation board ต้องคุมไฟ/พัดลมแบบ hysteresis + gate จาก main context |
| FR-09 | LINE bridge ต้องส่งต่อ event/status/ack ไป LINE ได้ |
| FR-10 | เมื่อเสี่ยงบุกรุกระดับสูง ต้องมีแจ้งเตือน LINE แบบชัดเจน |
| FR-11 | Keypad ต้องมีปุ่มขอความช่วยเหลือ และระบบต้องแจ้งเตือนออกไป |
| FR-12 | ระบบต้องทำงานได้ต่อเนื่องในเส้นทาง manual/auto ภายใต้ runtime ปกติ |

### 2.2 Non-Functional Requirements

| ID | ข้อกำหนด |
|---|---|
| NFR-01 | เส้นทาง security-critical ต้อง fail closed เมื่อ auth ไม่พร้อม |
| NFR-02 | การแจ้งเตือนซ้ำต้องมี cooldown ลด spam |
| NFR-03 | โครงสร้างต้อง modular แยกตามบทบาท board |
| NFR-04 | เฟิร์มแวร์ต้อง build ได้ทั้ง `main-board` และ `automation-board` |
| NFR-05 | bridge ต้องรองรับ unit test โดยไม่ต้องพึ่ง MQTT/LINE จริง |

## 3) รายละเอียดการทำงานปัจจุบัน (As-Built)

### 3.1 Main Board
- รับ event จาก sensor/keypad/serial/remote
- ประมวลผลด้วย `SecurityOrchestrator` + `RuleEngine`
- สั่งงาน actuator (servo/buzzer) และ publish MQTT

ไฟล์หลัก:
- `src/main_board/app/SecurityOrchestrator.cpp`
- `src/main_board/app/RuleEngine.cpp`
- `src/main_board/services/CommandDispatcher.cpp`

### 3.2 Automation Board
- Light auto: lux hysteresis + gate จาก main mode/presence
- Fan auto: temp hysteresis + gate แบบเดียวกับ light
- Publish auto status/ack และรับคำสั่ง auto cmd

ไฟล์หลัก:
- `src/auto_board/app/AutomationRuntime.cpp`
- `src/auto_board/automation/light_system.cpp`
- `src/auto_board/automation/temp_system.cpp`

### 3.3 LINE Bridge
- รับคำสั่งจาก LINE webhook/HTTP และส่งเข้าหัวข้อ MQTT command
- แปลง MQTT event/status/ack เป็นข้อความแจ้งเตือนใน LINE
- กรอง spam บางกรณี (เช่น ack ของ status)

ไฟล์หลัก:
- `tools/line_bridge/bridge.py`

### 3.4 อัปเดตล่าสุด

1. Intruder alert บน LINE แบบเฉพาะ
- ส่งข้อความ `[ALERT] possible intruder detected`
- trigger จาก level `alert/critical` + event/reason กลุ่มบุกรุก
- ตั้ง cooldown ผ่าน `INTRUDER_NOTIFY_COOLDOWN_S` (default 20 วินาที)

2. ปุ่มช่วยเหลือบน keypad
- ปุ่ม `B` -> event `keypad_help_request`
- Main publish event/status ด้วย reason `keypad_help_request`
- Bridge ส่งข้อความ `[HELP] keypad assistance requested`

## 4) แนวทางทดสอบ

ระดับทดสอบ:
- Unit test (bridge)
- Build/compile validation (firmware)
- Integration test (MQTT path)
- Hardware/manual test (sensor/actuator ของจริง)

## 5) กรณีทดสอบ (Test Cases)

### 5.1 Security และ Command

| TC ID | Requirement | เงื่อนไขก่อนทดสอบ | ขั้นตอน | ผลที่คาดหวัง |
|---|---|---|---|---|
| TC-CMD-001 | FR-05 | token พร้อม | ส่ง mutating cmd + nonce ถูกต้อง | รับคำสั่งและ ack ok |
| TC-CMD-002 | FR-05 | token พร้อม | ส่ง nonce เดิมซ้ำ | reject (replay) |
| TC-CMD-003 | FR-06 | mode != disarm | ส่ง `unlock door` | reject (`disarm required`) |
| TC-CMD-004 | FR-06 | door open | ส่ง `lock door` | reject (`door open`) |
| TC-RULE-001 | FR-03 | mode armed | trigger `door_open` | เกิด entry pending + warn |
| TC-RULE-002 | FR-03, FR-10 | mode armed | trigger `entry_timeout` | critical + notify/alert |

### 5.2 Keypad และ Help Request

| TC ID | Requirement | เงื่อนไขก่อนทดสอบ | ขั้นตอน | ผลที่คาดหวัง |
|---|---|---|---|---|
| TC-KP-001 | FR-11 | keypad ต่อใช้งาน | กด `B` | เกิด event `keypad_help_request` |
| TC-KP-002 | FR-11 | bridge + line target พร้อม | กด `B` | LINE ได้ `[HELP] keypad assistance requested` |
| TC-KP-003 | FR-07 | มี hold warning | กด `A` | warning ถูก silence |

### 5.3 LINE Alert

| TC ID | Requirement | เงื่อนไขก่อนทดสอบ | ขั้นตอน | ผลที่คาดหวัง |
|---|---|---|---|---|
| TC-LINE-001 | FR-09 | bridge พร้อม | ส่ง status/event ปกติ | LINE ได้ข้อความรูปแบบปกติ |
| TC-LINE-002 | FR-10 | bridge พร้อม | ส่ง event ระดับ alert | LINE ได้ `[ALERT] ...` |
| TC-LINE-003 | NFR-02 | cooldown ทำงาน | ส่ง alert ถี่ๆ | ข้อความถูก throttle |

### 5.4 Automation

| TC ID | Requirement | เงื่อนไขก่อนทดสอบ | ขั้นตอน | ผลที่คาดหวัง |
|---|---|---|---|---|
| TC-AUTO-001 | FR-08 | light auto on + context valid | ปรับ lux ข้าม threshold | ไฟเปลี่ยนตาม hysteresis |
| TC-AUTO-002 | FR-08 | fan auto on + context valid | ปรับ temp ข้าม threshold | พัดลมเปลี่ยนตาม hysteresis |
| TC-AUTO-003 | FR-08 | main gate blocked | เปิด auto ค้างไว้ | output ถูก force off |

## 6) ผลทดสอบที่รันแล้วใน workspace นี้

ผ่านแล้ว:
- `python -m platformio run -e main-board`
- `python -m platformio run -e automation-board`
- `python -m unittest test.bridge.test_bridge -v` (ผ่าน 12/12)
- `python -m py_compile tools/line_bridge/bridge.py`

ยังไม่ครอบคลุมในรอบนี้:
- hardware-in-loop ครบทุกเซ็นเซอร์จริง
- soak test ระยะยาวภายใต้ network ขาด/กลับ

## 7) งานค้างที่ควรทำต่อ

- เพิ่ม unit test สำหรับ branch ใหม่ของ bridge (intruder/help formatter)
- สรุป policy สุดท้ายของ trigger + cooldown สำหรับหน้างานจริง
- ทำ test matrix hardware/manual แบบเป็นระบบมากขึ้น

