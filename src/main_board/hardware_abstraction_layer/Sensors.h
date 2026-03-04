#pragma once

#include <Arduino.h>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/Types.h"

namespace HardwareAbstractionLayer {

class Sensors {
public:
  void begin(); // ตั้งค่าโหมด GPIO และค่าเริ่มต้นของสถานะเซนเซอร์
  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านอินพุตทั้งหมดตามลำดับที่กำหนดและส่งอีเวนต์แรกที่พบ
  bool isDoorOpen() const; // คืนค่าสถานะประตูเปิดแบบผ่านดีบาวซ์แล้ว
  bool isWindowOpen() const; // คืนค่าสถานะหน้าต่างเปิดแบบผ่านดีบาวซ์แล้ว

private:
  struct ReedState {
    bool stableOpen = false; // สถานะเปิด/ปิดที่ผ่านดีบาวซ์แล้ว
    bool lastRaw = false; // ค่าอ่านดิบล่าสุดจากพิน
    bool firedOpen = false; // เคยปล่อยอีเวนต์ "เปิด" ไปแล้วในรอบนี้หรือยัง
    uint32_t lastFlipMs = 0; // เวลาที่ค่า raw เปลี่ยนล่าสุด
  };

  struct UltrasonicState {
    uint8_t trigPin = ConfigurationSharedTypes::Config::PIN_UNUSED; // พิน trigger ของช่องอัลตราโซนิก
    uint8_t echoPin = ConfigurationSharedTypes::Config::PIN_UNUSED; // พิน echo ของช่องอัลตราโซนิก
    bool inside = false; // วัตถุยังอยู่ในโซนตรวจจับหรือไม่
    uint32_t nextSampleMs = 0; // เวลาถัดไปที่อนุญาตให้อ่านช่องนี้
    uint32_t lastFireMs = 0; // เวลาล่าสุดที่ปล่อยอีเวนต์จากช่องนี้
  };

  ReedState doorReed_{}; // สถานะรีดสวิตช์ประตู
  ReedState windowReed_{}; // สถานะรีดสวิตช์หน้าต่าง

  bool pirLast_[3] = {false, false, false}; // ค่า raw ล่าสุดของ PIR ทั้ง 3 ตัว
  uint32_t pirLastFireMs_[3] = {0, 0, 0}; // เวลาปล่อยอีเวนต์ล่าสุดของ PIR แต่ละตัว

  bool vibLast_ = false; // ค่า raw ล่าสุดของเซนเซอร์สั่น
  uint32_t vibLastFireMs_ = 0; // เวลาปล่อยอีเวนต์สั่นล่าสุด

  UltrasonicState us_[3]{}; // สถานะของอัลตราโซนิกทั้ง 3 ช่อง
  uint8_t usRoundRobinIdx_ = 0; // index ช่องอัลตราโซนิกที่จะอ่านในรอบถัดไป

  static bool reached_(uint32_t nowMs, uint32_t targetMs); // เช็กเวลาแบบรองรับการ overflow ของ millis()
  static bool pinConfigured_(uint8_t pin); // เช็กว่าพินถูกตั้งค่าใช้งานจริงและไม่ใช่ PIN_UNUSED
  static uint8_t pirPin_(uint8_t idx); // แปลงดัชนี PIR (0..2) เป็นหมายเลขพินจริง

  int readUltrasonicCm_(const UltrasonicState& us, uint32_t timeoutUs = ConfigurationSharedTypes::Config::US_ECHO_TIMEOUT_US) const; // อ่านระยะอัลตราโซนิกเป็นเซนติเมตร (คืน 999 เมื่อ timeout)

  bool pollDoor_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านรีดสวิตช์ประตูพร้อมดีบาวซ์และตรวจจับขอบสัญญาณ
  bool pollWindow_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านรีดสวิตช์หน้าต่างพร้อมดีบาวซ์และตรวจจับขอบสัญญาณ
  bool pollPir_(uint32_t nowMs, uint8_t idx, ConfigurationSharedTypes::Event& out); // อ่าน PIR รายช่องพร้อมคูลดาวน์เมื่อเกิดขอบขาขึ้น
  bool pollUltrasonic_(uint32_t nowMs, uint8_t idx, ConfigurationSharedTypes::Event& out); // อ่านอัลตราโซนิกรายช่องพร้อม hysteresis และคูลดาวน์
  bool pollVibration_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านเซนเซอร์สั่นพร้อมคูลดาวน์เมื่อเกิดขอบขาขึ้น
};

} // namespace HardwareAbstractionLayer
