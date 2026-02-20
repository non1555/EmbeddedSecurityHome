# Business Rules (ฉบับย่อภาษาไทย)

เอกสารหลักให้ใช้ `docs/business_rules.md`
ไฟล์นี้เป็นฉบับย่อเพื่ออ่านเร็วระหว่างเตรียมส่งงาน

## กฎสำคัญ

- BR-01..03: เปลี่ยนโหมดต้อง reset state เสี่ยงและ entry pending
- BR-04/04A/05: ถ้า door_open ตอน door_locked อยู่ ให้เป็น alert ทันที (ทุกโหมด); กรณี armed และไม่ได้ล็อก ให้เข้า entry delay และ escalate เมื่อ timeout
- BR-06..11: ประเมินความเสี่ยงด้วยคะแนน + correlation + decay + threshold ที่ตายตัว
- BR-12..16: door unlock session ต้อง auto-lock และมี warning/silence ที่ควบคุมได้
- BR-17..19: remote command ต้องผ่าน token/nonce และ replay guard; fail-closed เมื่อ storage ไม่พร้อม
- BR-20..22: unlock ต้องผ่าน safety gate (mode/sensor fault/physical precondition)
- BR-23: keypad lockout หลังใส่รหัสผิดครบ limit
- BR-24: serial synthetic commands ถูก block โดย default
- BR-25..28: ต้องมี telemetry ชัดเจน และ contract ปัจจุบันเป็น single-board
- BR-29..30: ตอน boot ให้ baseline เป็น `disarm`, restore โหมดที่ persist ไว้ได้เฉพาะค่าที่ valid; ถ้า invalid ต้อง fallback เป็น `disarm`
