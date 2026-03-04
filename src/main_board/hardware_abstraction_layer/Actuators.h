#pragma once

#include <Arduino.h>

#include "configuration_shared_types/Config.h"

namespace HardwareAbstractionLayer {

class Actuators {
public:
  void begin(); // เริ่มต้นอุปกรณ์ขับออก (บัซเซอร์และเซอร์โว) ให้อยู่สถานะเริ่มต้น
  void update(uint32_t nowMs); // เดิน state machine ของบัซเซอร์/เซอร์โวแบบไม่บล็อก

  void warn(); // เริ่มแพทเทิร์นเสียงเตือนระดับ warn
  void alert(); // เริ่มแพทเทิร์นเสียงเตือนระดับ alert ต่อเนื่อง
  void silence(); // หยุดเสียงบัซเซอร์ทันที

  void lockDoor(); // สั่งเซอร์โวประตูไปตำแหน่งล็อก
  void unlockDoor(); // สั่งเซอร์โวประตูไปตำแหน่งปลดล็อก
  void lockWindow(); // สั่งเซอร์โวหน้าต่างไปตำแหน่งล็อก
  void unlockWindow(); // สั่งเซอร์โวหน้าต่างไปตำแหน่งปลดล็อก
  void lockAll(); // สั่งล็อกทั้งประตูและหน้าต่าง
  void unlockAll(); // สั่งปลดล็อกทั้งประตูและหน้าต่าง

  bool isDoorLocked() const; // คืนค่า true เมื่อเซอร์โวประตูอยู่ตำแหน่งล็อก
  bool isWindowLocked() const; // คืนค่า true เมื่อเซอร์โวหน้าต่างอยู่ตำแหน่งล็อก

private:
  enum class BuzzerMode : uint8_t {
    idle, // ไม่ส่งเสียง
    warn, // โหมดเสียงเตือน warn
    alert // โหมดเสียงเตือน alert
  };

  struct ServoState {
    uint8_t pin = ConfigurationSharedTypes::Config::PIN_UNUSED; // พินที่ผูกกับเซอร์โวตัวนี้
    uint8_t channel = 0; // ช่อง PWM/LEDC ที่ใช้ขับเซอร์โว
    uint8_t lockDeg = 0; // มุมเซอร์โวตอนล็อก
    uint8_t unlockDeg = 0; // มุมเซอร์โวตอนปลดล็อก
    uint8_t currentDeg = 0; // มุมปัจจุบันของเซอร์โว
    uint8_t targetDeg = 0; // มุมเป้าหมายที่กำลังวิ่งไป
    uint32_t nextMoveMs = 0; // เวลาถัดไปที่อนุญาตให้ขยับ 1 step
  };

  BuzzerMode buzzerMode_ = BuzzerMode::idle; // โหมดเสียงปัจจุบันของบัซเซอร์
  bool buzzerToneOn_ = false; // สถานะเปิด/ปิด tone ปัจจุบัน
  uint32_t buzzerNextMs_ = 0; // เวลาถัดไปที่ต้องสลับสถานะเสียง
  uint8_t buzzerStep_ = 0; // ขั้นปัจจุบันของแพทเทิร์นเสียง

  ServoState doorServo_{}; // สถานะเซอร์โวฝั่งประตู
  ServoState windowServo_{}; // สถานะเซอร์โวฝั่งหน้าต่าง

  static bool reached_(uint32_t nowMs, uint32_t targetMs); // เช็กเวลาแบบรองรับการ overflow ของ millis()
  static bool pinConfigured_(uint8_t pin); // เช็กว่าพินถูกตั้งค่าใช้งานจริงและไม่ใช่ PIN_UNUSED

  void beginBuzzer_(); // ตั้งค่า LEDC channel สำหรับบัซเซอร์
  void setTone_(bool on, uint32_t hz); // เปิด/ปิดเสียงบัซเซอร์ด้วยความถี่ที่กำหนด
  void updateBuzzer_(uint32_t nowMs); // อัปเดตแพทเทิร์นเสียงบัซเซอร์ตามเวลา

  void setupServo_(ServoState& servo,
                   uint8_t pin,
                   uint8_t channel,
                   uint8_t lockDeg,
                   uint8_t unlockDeg); // ตั้งค่าโปรไฟล์เซอร์โว 1 ตัวและช่อง LEDC ที่ใช้งาน
  void writeServo_(const ServoState& servo, uint8_t deg) const; // เขียนมุมเซอร์โวแบบ absolute 1 ครั้ง
  void updateServo_(ServoState& servo, uint32_t nowMs); // ขยับเซอร์โวทีละ step ไปหามุมเป้าหมาย
};

} // namespace HardwareAbstractionLayer
