# Final Submission Checklist (TH)

ใช้เช็ครอบสุดท้ายก่อนส่งอาจารย์

## A) เนื้อหาเอกสารหลัก

- [ ] อัปเดตวันที่ใน `docs/project_description_requirements_details_testcases_th.md`
- [ ] อัปเดตวันที่ใน `docs/architectural_design_actual_update_draft_th.md`
- [ ] อัปเดตวันที่ใน `docs/gantt_chart_th.md`
- [ ] ยืนยันว่าหัวข้อใน `docs/project_presentation_abstract.md` ครบ:
- [ ] ลักษณะชิ้นงาน
- [ ] การใช้งาน
- [ ] แผนการทำงาน
- [ ] hardware design
- [ ] circuit
- [ ] flowchart
- [ ] code
- [ ] test result

## B) Flowchart และภาพประกอบ

- [ ] เปิด `docs/flowcharts.md` แล้ว Mermaid render ได้ทุกบล็อก
- [ ] เปิด `docs/full_system_flow_detailed.svg` แล้วอ่านได้ครบ
- [ ] เปิด `docs/block_diagram.md` แล้ว Mermaid render ได้ครบทุกแผนภาพ
- [ ] มีไฟล์ภาพที่พร้อมแนบรายงาน: block diagram, circuit, flowchart

## C) หลักฐานทดสอบ

- [ ] รัน `python -m platformio run -e main-board`
- [ ] รัน `python -m platformio run -e automation-board`
- [ ] รัน `python -m unittest test.bridge.test_bridge -v`
- [ ] รัน `python -m py_compile tools/line_bridge/bridge.py`
- [ ] กรอกผลลง `docs/submission_pack/03_test_evidence_template_th.md`

## D) แพ็กส่งงาน

- [ ] export เอกสารนำเสนอเป็น PDF
- [ ] export เอกสาร description เป็น PDF
- [ ] export เอกสาร architecture เป็น PDF
- [ ] export gantt chart เป็นภาพ/PDF
- [ ] ตรวจชื่อไฟล์ส่งให้อ่านง่ายและสื่อความหมาย
- [ ] ตรวจลิงก์ภายใน README/docs ว่าไม่เสีย
