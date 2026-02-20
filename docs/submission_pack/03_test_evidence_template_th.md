# Test Evidence Template (Single-Board)

| วันที่ (YYYY-MM-DD) | คำสั่งทดสอบ | ผล (PASS/FAIL) | หลักฐาน (ไฟล์/รูป/ลิงก์) | หมายเหตุ |
|---|---|---|---|---|
| 2026-02-19 | `python -m platformio run -e main-board` | PASS/FAIL | `docs/evidence/...` | build baseline |
| YYYY-MM-DD | TC-01 Disarm reset | PASS/FAIL | `docs/evidence/...` | อ้าง BR-01 |
| YYYY-MM-DD | TC-02 Entry timeout alert | PASS/FAIL | `docs/evidence/...` | อ้าง BR-04, BR-05 |
| YYYY-MM-DD | TC-05 Unlock timeout relock | PASS/FAIL | `docs/evidence/...` | อ้าง BR-12 |
| YYYY-MM-DD | TC-08 Remote unauthorized reject | PASS/FAIL | `docs/evidence/...` | อ้าง BR-17, BR-18 |
| YYYY-MM-DD | TC-12 Keypad lockout | PASS/FAIL | `docs/evidence/...` | อ้าง BR-23 |

หมายเหตุ: เพิ่มแถวได้ตามจำนวน test case ที่รันจริง
