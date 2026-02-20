# Final Checklist (Single-Board)

- [ ] ยืนยันขอบเขตส่งงานเป็นบอร์ดเดียว (`main-board`)
- [ ] ยืนยันเอกสาร active อยู่ใน `docs/` และเอกสารเก่าอยู่ใน `docs/legacy_2026-02-19/`
- [ ] ตรวจว่า Business Rules ล่าสุดอยู่ใน `docs/business_rules.md`
- [ ] ตรวจว่า Requirement/Testcase mapping ล่าสุดอยู่ใน `docs/project_description_requirements_details_testcases_th.md`
- [ ] ตรวจว่า flow ล่าสุดอยู่ใน `docs/full_system_flow_detailed.mmd`
- [ ] รัน build สำเร็จ: `python -m platformio run -e main-board`
- [ ] ตรวจว่า launcher/tool ไม่มีปุ่มหรือ env สำหรับบอร์ดที่สอง
- [ ] บันทึกผลทดสอบใน `docs/submission_pack/03_test_evidence_template_th.md`
- [ ] ตรวจ git diff รอบสุดท้ายให้เหลือเฉพาะสิ่งที่ต้องส่ง
