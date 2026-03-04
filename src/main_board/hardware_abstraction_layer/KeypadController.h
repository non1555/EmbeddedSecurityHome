#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/Types.h"

namespace HardwareAbstractionLayer {

class KeypadController {
public:
  void begin(); // เริ่มต้นสถานะตัวสแกนคีย์แพดและโหลดรหัสประตูที่ตั้งไว้
  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านคีย์แพดและปล่อยอีเวนต์เชิงความหมายตามปุ่มที่กด
  bool consumeInputActivity(); // คืนค่าว่ามีการแก้ไขบัฟเฟอร์คีย์แพดใน tick นี้หรือไม่ แล้วเคลียร์แฟลก

  const char* buffer() const; // คืน pointer ของบัฟเฟอร์ PIN ปัจจุบัน
  uint8_t length() const; // คืนจำนวนหลัก PIN ที่ป้อนอยู่ตอนนี้
  bool consumeSubmitResult(bool& ok); // ดึงผล submit ล่าสุดครั้งเดียวแล้วเคลียร์แฟลก

private:
  char doorCode_[5] = {0, 0, 0, 0, 0}; // รหัสประตูที่ใช้อ้างอิง (4 หลัก + null)
  char inputBuffer_[5] = {0, 0, 0, 0, 0}; // บัฟเฟอร์รหัสที่ผู้ใช้กำลังกด
  uint8_t inputLength_ = 0; // ความยาวข้อมูลใน inputBuffer_ ปัจจุบัน

  bool submitReady_ = false; // มีผล submit ใหม่พร้อมให้ consumer อ่านหรือไม่
  bool submitOk_ = false; // ผล submit ล่าสุดถูกต้องหรือไม่
  bool inputEdited_ = false; // มีการแก้ไขบัฟเฟอร์ใน tick ล่าสุดหรือไม่

  uint8_t scanRow_ = 0; // แถวที่กำลังสแกนในรอบปัจจุบัน
  bool waitingRelease_ = false; // รอปล่อยปุ่มก่อนรับคีย์ใหม่หรือไม่
  char lastKey_ = 0; // คีย์ล่าสุดที่ผ่าน debounce แล้ว
  uint32_t lastKeyMs_ = 0; // เวลาที่อ่านคีย์ล่าสุด
  uint8_t ioShadow_ = 0xFF; // เงา bit state ของพอร์ต PCF8574 ล่าสุด

  void clear_(); // ล้างบัฟเฟอร์ PIN ที่ป้อนอยู่
  void setDoorCodeFromBuild_(); // โหลด DOOR_CODE จาก build config หรือใช้ค่าเริ่มต้น
  bool isValidCode_(const char* code) const; // ตรวจว่ารหัสเป็นเลข 4 หลักถูกฟอร์แมต
  bool matchesDoorCode_() const; // เปรียบเทียบ PIN ที่กดกับรหัสประตูจริง

  bool writePort_(uint8_t value); // เขียนค่าดิบ 1 ไบต์ไปยัง I2C expander ของคีย์แพด
  int readPressedColumn_(); // อ่านคอลัมน์ที่ถูกกดจากเมทริกซ์ (หรือ -1 ถ้าไม่พบ)
  bool setRowActive_(uint8_t row); // ดึงแถวที่ระบุลง low เพื่อสแกนทีละแถว
  bool setAllRowsHigh_(); // ปล่อยทุกแถวเป็น high (สถานะ idle)
  char mapKey_(uint8_t row, uint8_t col) const; // แมปตำแหน่งแถว/คอลัมน์เป็นตัวอักษรปุ่ม
  char scanKey_(uint32_t nowMs); // รันสแกนคีย์ 1 รอบพร้อม debounce และรอปล่อยปุ่ม
};

} // namespace HardwareAbstractionLayer
