#pragma once

#include <Arduino.h>

namespace ConfigurationSharedTypes {

enum class Mode : uint8_t {
  disarm, // โหมดปลดอาวุธ
  away, // โหมดเฝ้าระวังเมื่อไม่อยู่บ้าน
  night // โหมดเฝ้าระวังกลางคืน (เน้น perimeter)
};

enum class AlarmLevel : uint8_t {
  off, // ไม่มีการเตือน
  warn, // เตือนระดับเบา
  alert // เตือนระดับรุนแรง
};

enum class EventType : uint8_t {
  disarm, // อีเวนต์ปลดอาวุธ
  arm_away, // อีเวนต์เข้าโหมด away
  arm_night, // อีเวนต์เข้าโหมด night
  door_open, // ตรวจพบประตูเปิด
  window_open, // ตรวจพบหน้าต่างเปิด
  vib_spike, // ตรวจพบแรงสั่นสะเทือนผิดปกติ
  motion, // ตรวจพบการเคลื่อนไหวจาก PIR
  chokepoint, // ตรวจพบการผ่านจุด choke point จากอัลตราโซนิก
  door_hold_warn_silence, // คำสั่งปิดเสียงเตือนประตูเปิดค้าง
  keypad_help_request, // คำขอความช่วยเหลือจากคีย์แพด/LINE
  door_code_unlock, // ป้อนรหัสถูกต้องและขอปลดล็อกประตู
  door_code_bad, // ป้อนรหัสผิด
  manual_door_toggle, // กดปุ่มสลับล็อก/ปลดล็อกประตู
  manual_window_toggle // กดปุ่มสลับล็อก/ปลดล็อกหน้าต่าง
};

enum class CommandType : uint8_t {
  none, // ไม่มีคำสั่ง actuator เพิ่มเติม
  buzzer_warn, // สั่งบัซเซอร์โหมด warn
  buzzer_alert, // สั่งบัซเซอร์โหมด alert
  buzzer_stop // สั่งหยุดบัซเซอร์
};

struct Event {
  EventType type = EventType::disarm; // ประเภทอีเวนต์
  uint32_t ts_ms = 0; // เวลาเกิดอีเวนต์ (มิลลิวินาที)
  uint8_t src = 0; // แหล่งที่มาของอีเวนต์ (เช่น id เซนเซอร์/แหล่งคำสั่ง)

  constexpr Event() = default; // สร้างอีเวนต์ค่าเริ่มต้น (disarm, ts=0, src=0)
  constexpr Event(EventType t, uint32_t ts, uint8_t s = 0)
  : type(t), ts_ms(ts), src(s) {} // สร้างอีเวนต์โดยกำหนด type/time/source ชัดเจน
};

struct SystemState {
  Mode mode = Mode::disarm; // โหมดที่ active อยู่ตอนนี้
  Mode latest_mode = Mode::disarm; // โหมดพื้นฐานล่าสุด (disarm/away)
  bool is_night = false; // flag ว่าเปิด night override อยู่หรือไม่
  AlarmLevel level = AlarmLevel::off; // ระดับการเตือนปัจจุบัน

  uint8_t failed_attempts = 0; // จำนวนครั้งป้อนรหัสผิดสะสม

  uint8_t exit_stage = 0; // ขั้นของ workflow auto-arm
  uint32_t exit_timeout_ms = 0; // เวลา timeout ของขั้น auto-arm ปัจจุบัน

  bool door_locked = false; // สถานะล็อกประตู
  bool window_locked = false; // สถานะล็อกหน้าต่าง
  bool door_open = false; // สถานะประตูเปิด
  bool window_open = false; // สถานะหน้าต่างเปิด
};

struct Decision {
  SystemState next{}; // state ถัดไปหลังประมวลผลกฎ
  CommandType cmd = CommandType::none; // คำสั่ง actuator ที่ต้องทำเพิ่มจาก state
  const char* flag = ""; // ป้ายเหตุผล/นโยบายที่ใช้ส่งต่อ telemetry
};

enum class PublishKind : uint8_t {
  event, // ข้อความประเภท event
  status, // ข้อความประเภท status
  ack // ข้อความประเภทตอบรับคำสั่ง
};

struct PublishMessage {
  PublishKind kind = PublishKind::event; // ประเภทข้อความ MQTT ที่จะส่ง
  Event event{}; // payload event (ใช้เมื่อ kind=event)
  SystemState st{}; // snapshot state ที่แนบไปกับข้อความ
  bool ok = false; // สถานะสำเร็จ/ไม่สำเร็จ (ใช้กับ ack)
  char text1[32]{}; // ข้อความสั้นช่องที่ 1 (reason/cmd/flag)
  char text2[64]{}; // ข้อความสั้นช่องที่ 2 (detail)
};

struct RemoteCommandMessage {
  char payload[128]{}; // payload คำสั่งดิบที่รับมาจาก MQTT
};

inline const char* toString(Mode m) { // แปลง Mode เป็นข้อความมาตรฐานสำหรับ log/JSON
  switch (m) {
    case Mode::disarm: return "disarm";
    case Mode::away: return "away";
    case Mode::night: return "night";
    default: return "unknown";
  }
}

inline const char* toString(AlarmLevel lv) { // แปลง AlarmLevel เป็นข้อความมาตรฐานสำหรับ log/JSON
  switch (lv) {
    case AlarmLevel::off: return "off";
    case AlarmLevel::warn: return "warn";
    case AlarmLevel::alert: return "alert";
    default: return "unknown";
  }
}

inline const char* toString(EventType t) { // แปลง EventType เป็นข้อความมาตรฐานสำหรับ log/JSON
  switch (t) {
    case EventType::disarm: return "disarm";
    case EventType::arm_away: return "arm_away";
    case EventType::arm_night: return "arm_night";
    case EventType::door_open: return "door_open";
    case EventType::window_open: return "window_open";
    case EventType::vib_spike: return "vib_spike";
    case EventType::motion: return "motion";
    case EventType::chokepoint: return "chokepoint";
    case EventType::door_hold_warn_silence: return "door_hold_warn_silence";
    case EventType::keypad_help_request: return "keypad_help_request";
    case EventType::door_code_unlock: return "door_code_unlock";
    case EventType::door_code_bad: return "door_code_bad";
    case EventType::manual_door_toggle: return "manual_door_toggle";
    case EventType::manual_window_toggle: return "manual_window_toggle";
    default: return "unknown";
  }
}

inline const char* toString(CommandType t) { // แปลง CommandType เป็นข้อความมาตรฐานสำหรับ log/JSON
  switch (t) {
    case CommandType::none: return "none";
    case CommandType::buzzer_warn: return "buzzer_warn";
    case CommandType::buzzer_alert: return "buzzer_alert";
    case CommandType::buzzer_stop: return "buzzer_stop";
    default: return "unknown";
  }
}

} // namespace ConfigurationSharedTypes
