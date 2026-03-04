#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/Types.h"
#include "hardware_abstraction_layer/DisplayManager.h"
#include "hardware_abstraction_layer/KeypadController.h"
#include "hardware_abstraction_layer/Sensors.h"

namespace ApplicationLogicLayer {

class EventCollector {
public:
  void begin(); // เริ่มต้น I2C, เซนเซอร์, คีย์แพด และจอแสดงผล

  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านอินพุตตามลำดับหลัก (keypad ก่อน แล้วปุ่ม/เซนเซอร์) และคืนอีเวนต์แรกที่พบ
  bool pollKeypad(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านคีย์แพดและคืนอีเวนต์เมื่อมีเหตุการณ์สำคัญจากการกดปุ่ม
  bool pollSensorsOrSerial(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านปุ่ม manual ก่อน แล้วอ่านเซนเซอร์เพื่อคืนอีเวนต์ถัดไป

  bool isDoorOpen() const; // คืนค่าสถานะประตูเปิดแบบผ่านดีบาวซ์แล้ว
  bool isWindowOpen() const; // คืนค่าสถานะหน้าต่างเปิดแบบผ่านดีบาวซ์แล้ว

  void updateDisplay(uint32_t nowMs,
                     bool doorLocked,
                     bool doorOpen,
                     bool countdownActive,
                     uint32_t countdownDeadlineMs,
                     uint32_t countdownWarnBeforeMs); // อัปเดตข้อมูลที่จอ OLED ต้องใช้แสดง (สถานะประตูและนับถอยหลัง)

private:
  bool pollManualButtons_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านปุ่ม toggle บนบอร์ดพร้อมดีบาวซ์และปล่อยอีเวนต์ manual
  bool pollManualButton_(uint8_t pin,
                         uint32_t nowMs,
                         bool& lastRawPressed,
                         bool& stablePressed,
                         uint32_t& lastChangeMs,
                         ConfigurationSharedTypes::EventType pressEvent,
                         ConfigurationSharedTypes::Event& out); // อ่านปุ่ม local 1 ตัวด้วย edge debounce แล้วปล่อยอีเวนต์เมื่อกดค้างเสถียร

  HardwareAbstractionLayer::Sensors sensors_; // โมดูลอ่านเซนเซอร์ทั้งหมด
  HardwareAbstractionLayer::KeypadController keypad_; // โมดูลจัดการคีย์แพด
  HardwareAbstractionLayer::DisplayManager display_; // โมดูลแสดงผล OLED
  bool doorToggleLastRawPressed_ = false; // ค่า raw ล่าสุดของปุ่ม toggle ประตู
  bool doorToggleStablePressed_ = false; // ค่าปุ่ม toggle ประตูหลังดีบาวซ์
  uint32_t doorToggleLastChangeMs_ = 0; // เวลาที่ปุ่ม toggle ประตูเปลี่ยนสถานะล่าสุด
  bool windowToggleLastRawPressed_ = false; // ค่า raw ล่าสุดของปุ่ม toggle หน้าต่าง
  bool windowToggleStablePressed_ = false; // ค่าปุ่ม toggle หน้าต่างหลังดีบาวซ์
  uint32_t windowToggleLastChangeMs_ = 0; // เวลาที่ปุ่ม toggle หน้าต่างเปลี่ยนสถานะล่าสุด
};

} // namespace ApplicationLogicLayer
