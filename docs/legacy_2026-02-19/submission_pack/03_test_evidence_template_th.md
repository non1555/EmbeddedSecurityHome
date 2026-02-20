# Test Evidence Template (TH)

ไฟล์นี้ใช้เก็บหลักฐานผลทดสอบรอบส่งงานให้ครบในที่เดียว

## 1) Build และ Unit Test

| วันที่ | คำสั่ง | ผลลัพธ์ | หลักฐาน (ไฟล์/ภาพ) | หมายเหตุ |
|---|---|---|---|---|
| YYYY-MM-DD | `python -m platformio run -e main-board` | PASS/FAIL | `docs/evidence/...` | |
| YYYY-MM-DD | `python -m platformio run -e automation-board` | PASS/FAIL | `docs/evidence/...` | |
| YYYY-MM-DD | `python -m unittest test.bridge.test_bridge -v` | PASS/FAIL | `docs/evidence/...` | |
| YYYY-MM-DD | `python -m py_compile tools/line_bridge/bridge.py` | PASS/FAIL | `docs/evidence/...` | |

## 2) Functional Test Cases

| TC ID | Scenario | ขั้นตอนทดสอบย่อ | Expected | Actual | PASS/FAIL | หลักฐาน |
|---|---|---|---|---|---|---|
| TC-CMD-001 | token+nonce command | ส่งคำสั่ง mutating ผ่าน LINE/MQTT | ack ok | | | |
| TC-CMD-002 | replay nonce | ส่ง nonce ซ้ำ | reject | | | |
| TC-RULE-002 | entry timeout | เปิดประตูใน armed แล้วไม่ disarm | critical + notify | | | |
| TC-LINE-002 | intruder alert | trigger event กลุ่ม intrusion | LINE ได้ `[ALERT] ...` | | | |
| TC-KP-001 | keypad help | กดปุ่ม `B` | event `keypad_help_request` | | | |
| TC-KP-002 | help notify | กดปุ่ม `B` แล้วดู LINE | LINE ได้ `[HELP] ...` | | | |
| TC-AUTO-001 | light auto hysteresis | ปรับ lux ข้าม threshold | light เปลี่ยนตาม hysteresis | | | |
| TC-AUTO-002 | fan auto hysteresis | ปรับ temp ข้าม threshold | fan เปลี่ยนตาม hysteresis | | | |

## 3) สรุปความพร้อมใช้งาน

- จำนวน test case ที่รัน:
- จำนวนที่ผ่าน:
- จำนวนที่ไม่ผ่าน:
- ประเด็นที่ต้องจูนเพิ่มก่อน demo:
