1. Layered Software Architecture

ชั้นที่ 1: Configuration & Shared Types (ชั้นตั้งค่าและชนิดข้อมูลร่วม)

Config.h: เก็บค่าคงที่ทั้งหมด เช่น pin assignment, time constants, network settings และพารามิเตอร์ RTOS

Types.h: นิยาม enum และโครงสร้างข้อมูลกลางของระบบ (Mode, AlarmLevel, EventType, Event, SystemState ฯลฯ) ให้ทุกไฟล์ใช้มาตรฐานเดียวกัน

ชั้นที่ 2: Hardware Abstraction Layer (HAL) ชั้นจัดการอุปกรณ์

กฎหลัก: ชั้นนี้ห้ามมีตรรกะตัดสินใจด้านความปลอดภัย มีหน้าที่อ่าน/เขียนฮาร์ดแวร์เท่านั้น

Sensors.h / .cpp: อ่านค่าจาก reed, PIR, vibration และ ultrasonic โดยใช้ Round-Robin (usIndex) และกำหนด pulseIn timeout 6ms เพื่อไม่บล็อก RTOS

Actuators.h / .cpp: ควบคุมอุปกรณ์เอาต์พุต เช่น servo lock/unlock และ buzzer pattern

KeypadController.h / .cpp: สแกนปุ่มเมทริกซ์และจัดการ PIN buffer (อัปเดตลอจิก: ปุ่ม * = Backspace ลบตัวท้าย 1 ตัว, ปุ่ม C = Clear ล้างทั้ง buffer)

DisplayManager.h / .cpp: แสดงผลสถานะระบบและผลการกด PIN บนจอ OLED ผ่าน I2C

ชั้นที่ 3: Core Services ชั้นบริการหลังบ้าน

MqttService.h / .cpp: จัดการ WiFi/MQTT แบบ non-blocking ภายในลูปของ SecTask (ไม่แยก mqttTask) พร้อมคิว publish/ack/status และรับคำสั่ง remote ผ่าน RTOS queue

NvsStorage.h / .cpp: จัดการ Preferences (NVS) ด้วยรูปแบบ State + Modifier โดยบันทึก latest_mode และ is_night แยกกันเพื่อรองรับ recovery หลังไฟดับ

ชั้นที่ 4: Application & Logic Layer ชั้นสมองกล

SystemContext.h / .cpp: ศูนย์รวมสถานะระบบปัจจุบัน จัดการ mode/level, door session, timeout, NVS persist และนโยบาย remote command (arm_night, night_off)

EventCollector.h / .cpp: ดึง event จาก Keypad/Sensors แล้วแปลงเป็น Event มาตรฐานให้ RuleEngine ใช้งาน

RuleEngine.h / .cpp: state machine หลักที่ประมวลผล event เพื่อเปลี่ยน mode/level และออกคำสั่ง actuator ตาม flowchart

ไฟล์หลัก: Entry Point

main.cpp: มี setup() และ loop() เพื่อรัน SecTask (Task เดียว) โดยในหนึ่งรอบ 20ms จะเรียก MQTT tick, กวาด remote command, poll Keypad/Sensors, ตรวจ timeout, ประมวลผล event queue ด้วย RuleEngine และอัปเดต actuator พร้อม WDT monitor
