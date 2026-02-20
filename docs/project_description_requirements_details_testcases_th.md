# ข้อกำหนด, Business Rules และ Test Cases (Single-Board)

## 1. ขอบเขต

เอกสารฉบับนี้รองรับเฟิร์มแวร์เป้าหมายเดียว:

- `main-board` เท่านั้น

เอกสารเดิมแบบหลายบอร์ดถูกเก็บไว้ที่ `docs/legacy_2026-02-19/`

## 2. Functional Requirements

| Req ID | ข้อกำหนด | Mapping ไป Business Rules |
|---|---|---|
| FR-01 | ระบบต้องรองรับโหมด `away`, `disarm` | BR-01, BR-03 |
| FR-02 | ระบบต้องประเมินความเสี่ยงจากเหตุการณ์ด้วยคะแนนที่กำหนดแน่นอน | BR-06, BR-07, BR-08, BR-09, BR-10, BR-11 |
| FR-03 | ระบบต้องจัดการ entry delay และ timeout escalation เมื่ออยู่ในโหมด armed | BR-04, BR-05 |
| FR-04 | ระบบต้องบังคับเงื่อนไข lock/unlock ตามสถานะประตู/หน้าต่างจริง | BR-12, BR-13, BR-21, BR-22 |
| FR-05 | คำสั่ง remote ต้องถูกป้องกันด้วย token/nonce | BR-17, BR-18, BR-19 |
| FR-06 | เมื่อพบ sensor fault ระบบต้อง fail-closed สำหรับการ unlock | BR-20 |
| FR-07 | ต้องมี keypad lockout ตามจำนวนครั้งที่ใส่รหัสผิด | BR-23 |
| FR-08 | ต้องส่ง telemetry MQTT (event/status/ack/metrics) เพื่อการติดตามระบบ | BR-25, BR-26, BR-27 |
| FR-09 | ระบบต้องคง continuity ของโหมดข้าม reboot โดย restore จาก persisted mode ที่ valid และ fallback แบบปลอดภัย | BR-29, BR-30 |

## 3. Non-Functional Requirements

| Req ID | ข้อกำหนด | Mapping ไป Business Rules |
|---|---|---|
| NFR-01 | พฤติกรรมต้อง deterministic เมื่อ input และ config เหมือนเดิม | BR-10, BR-11 |
| NFR-02 | ต้อง default เป็น deny/fail-closed เมื่อข้อมูล auth/storage ไม่พร้อม | BR-17, BR-18, BR-19, BR-20 |
| NFR-03 | baseline ส่งงานต้อง build ที่บอร์ดเดียว | BR-28 |
| NFR-04 | ต้องมี observability ทั้งแบบ periodic และ event-driven | BR-25, BR-26, BR-27 |
| NFR-05 | Boot behavior ต้อง deterministic หลังไฟดับ/รีบูต และห้าม auto-arm เมื่อ persisted mode invalid | BR-29, BR-30 |

## 4. Acceptance Test Matrix

| TC ID | สถานการณ์ | ขั้นตอนทดสอบ | ผลที่คาดหวัง | Rules |
|---|---|---|---|---|
| TC-01 | Disarm reset | arm ระบบ, ทำให้ score สูง, ส่ง `disarm` | mode เป็น disarm, score=0, ยกเลิก entry pending | BR-01 |
| TC-02 | Entry timeout alert | โหมด armed + door open แล้วรอ timeout | level เป็น alert และ score ถูกบังคับเป็น 100 พร้อม buzzer alert | BR-04, BR-05 |
| TC-03 | Correlation scoring | outdoor motion แล้ว window open ในช่วง correlation | คะแนนเพิ่มพร้อม bonus และ level อัปเดต | BR-06, BR-11 |
| TC-04 | Vibration high-risk path | trigger เหตุการณ์ให้คะแนนถึงช่วงเสี่ยงสูง (>=80) ทาง vib path | ยกเลิก entry pending และคงระดับผู้ใช้เป็น alert | BR-08, BR-11 |
| TC-05 | Unlock timeout relock | unlock ประตูในโหมด disarm แล้วไม่เปิดประตู | ประตู auto-lock เมื่อครบเวลา | BR-12, BR-14 |
| TC-06 | Open-close relock | unlock แล้วเปิดและปิดประตู | ประตู auto-lock หลังจาก close delay | BR-13 |
| TC-07 | Hold warning silence | เปิดประตูค้างจนเกิด hold warning แล้วกด silence | buzzer หยุดและ session ยังอยู่ใน state ที่ถูกต้อง | BR-15, BR-16 |
| TC-08 | Remote unauthorized reject | ส่งคำสั่งแก้สถานะโดยไม่มี token/nonce ที่ถูกต้อง | ACK reject และ status ระบุเหตุผล reject | BR-17, BR-18 |
| TC-09 | Nonce persistence fail-closed | บังคับโหมด strict แล้วไม่สามารถ persist nonce | คำสั่งถูกปฏิเสธด้วยเหตุผล storage ไม่พร้อม | BR-19 |
| TC-10 | Sensor fault unlock reject | ทำให้เกิด sensor fault แล้วสั่ง unlock | ระบบปฏิเสธการ unlock และ publish เหตุผล | BR-20 |
| TC-11 | Lock precondition reject | สั่ง lock ตอนประตู/หน้าต่างยังเปิด | คำสั่งถูกปฏิเสธ ไม่ lock actuator | BR-21 |
| TC-12 | Keypad lockout | ใส่รหัสผิดจนครบ limit แล้วใส่รหัสถูกระหว่าง lockout | ระบบยังไม่ปลดล็อกจนกว่า lockout หมดเวลา | BR-23 |
| TC-13 | Serial hardening | ส่ง serial synthetic cmd ภายใต้ config ค่าเริ่มต้น | คำสั่งถูก block และ publish สถานะ block | BR-24 |
| TC-14 | Telemetry contract | ตรวจ payload event/status ระหว่าง runtime | payload เป็น single-board contract ไม่มี dependency auto-board | BR-26, BR-27, BR-28 |
| TC-15 | Mode restore after reboot | ตั้งโหมดเป็น `away`, reboot board, แล้วดู status แรกหลัง boot | โหมดต้อง restore เป็น `away`; score/entry state ถูก reset ตาม baseline | BR-29, BR-30 |
| TC-16 | Invalid persisted mode fallback | ทำ persisted mode ให้เป็นค่าที่ไม่ valid แล้ว reboot | ระบบ fallback เป็น `disarm` และมี warning telemetry/message; ไม่ auto-arm | BR-29, BR-30 |

## 5. Build Verification

- `python -m platformio run -e main-board`
