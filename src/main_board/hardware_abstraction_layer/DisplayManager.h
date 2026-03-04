#pragma once

#include <Arduino.h>

class Adafruit_SSD1306;

namespace HardwareAbstractionLayer {

class DisplayManager {
public:
  bool begin(); // เริ่มต้นจอ OLED และแสดงหน้าจอเริ่มต้น
  void showCode(const char* code, uint8_t length); // อัปเดตข้อความ PIN ที่กำลังป้อนบนจอ
  void showSubmitResult(bool ok); // แสดงผล submit ชั่วคราวว่า OK/ERR
  void updateDoorStatus(uint32_t nowMs,
                        bool doorLocked,
                        bool doorOpen,
                        bool countdownActive,
                        uint32_t countdownDeadlineMs,
                        uint32_t countdownWarnBeforeMs); // อัปเดต cache สถานะประตู/นับถอยหลัง แล้วรีเฟรชจอเมื่อจำเป็น
  void tick(uint32_t nowMs); // อัปเดตงานแสดงผลตามเวลา (นับถอยหลัง/หมดเวลาแสดงผล)

private:
  Adafruit_SSD1306* display_ = nullptr; // pointer ไปยังอ็อบเจ็กต์จอ OLED

  char code_[5] = {0, 0, 0, 0, 0}; // ข้อความ PIN ล่าสุดสำหรับแสดงบนจอ
  uint8_t codeLength_ = 0; // ความยาว PIN ที่ต้องแสดง

  bool showingResult_ = false; // ตอนนี้อยู่ในโหมดแสดงผล submit ชั่วคราวหรือไม่
  bool resultOk_ = false; // ผล submit ล่าสุด (true=ok, false=err)
  uint32_t resultUntilMs_ = 0; // เวลาสิ้นสุดการแสดงผล submit ชั่วคราว

  bool doorLocked_ = false; // สถานะล็อกประตูล่าสุดที่ใช้แสดงผล
  bool doorOpen_ = false; // สถานะเปิดประตูล่าสุดที่ใช้แสดงผล
  bool countdownActive_ = false; // มี countdown ให้แสดงอยู่หรือไม่
  uint32_t countdownDeadlineMs_ = 0; // เวลาสิ้นสุด countdown
  uint32_t countdownWarnBeforeMs_ = 0; // threshold ที่ถือว่าเข้าโซนเตือนของ countdown

  int lastCountdownSec_ = -1; // วินาที countdown ล่าสุดที่วาดไปแล้ว
  bool lastCountdownUrgent_ = false; // สถานะเร่งด่วนของ countdown ล่าสุดที่วาดไปแล้ว
  bool dirty_ = true; // ธงว่าต้อง render ใหม่หรือไม่

  void render_(); // วาดเฟรม OLED ทั้งหมดจาก cache สถานะ UI ปัจจุบัน
};

} // namespace HardwareAbstractionLayer
