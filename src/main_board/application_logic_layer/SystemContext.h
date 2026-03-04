#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "core_services/MqttService.h"
#include "core_services/NvsStorage.h"
#include "configuration_shared_types/Types.h"
#include "hardware_abstraction_layer/Actuators.h"

namespace ApplicationLogicLayer {

class EventCollector;
class RuleEngine;

class SystemContext {
public:
  bool begin(); // เริ่มต้นบริการทั้งหมด, RTOS queue, actuator และโหลดสถานะที่บันทึกไว้
  void bindCollector(EventCollector* collector); // ผูกตัวเก็บเหตุการณ์เพื่อใช้อ่านเซนเซอร์และอัปเดตจอ

  bool pollEvent(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // ดึงอีเวนต์ 1 รายการจาก remote หรือ collector
  bool pollRemoteEvent(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // ดึงคำสั่งจาก MQTT แล้วแมปเป็นอีเวนต์ถ้าทำได้
  void securityTick(uint32_t nowMs,
                    RuleEngine& engine,
                    QueueHandle_t eventQueue,
                    uint8_t maxRemoteEventsPerTick = 1); // รัน 1 รอบของลูปความปลอดภัย: drain remote/local, process rule, update actuator และ log serial
  void applyDecision(const ConfigurationSharedTypes::Event& event, const ConfigurationSharedTypes::Decision& decision); // นำผลตัดสินใจจาก RuleEngine ไปอัปเดต state/hardware/NVS/MQTT
  void updateActuators(uint32_t nowMs, const RuleEngine& engine); // อัปเดตรอบคาบของ actuator, auto-arm tick, display และ periodic status
  void mqttTick(uint32_t nowMs); // ให้บริการ MQTT 1 รอบ (reconnect, receive, publish queue)
  bool isMqttConnected(); // คืนค่าสถานะเชื่อมต่อ MQTT ปัจจุบัน

  void handleSilenceRequest(); // จัดการคำสั่งเงียบเสียงเตือน
  void handleHelpRequest(const ConfigurationSharedTypes::Event& event); // จัดการอีเวนต์ขอความช่วยเหลือ (ยกระดับเป็น alert และแจ้ง MQTT)
  void handleManualToggle(const ConfigurationSharedTypes::Event& event); // จัดการปุ่มสลับล็อก/ปลดล็อกที่ตัวอุปกรณ์

  ConfigurationSharedTypes::SystemState& state(); // คืน state แบบแก้ไขได้
  const ConfigurationSharedTypes::SystemState& state() const; // คืน state แบบอ่านอย่างเดียว

private:
  struct DoorSession {
    bool active = false; // มี session ปลดล็อกค้างอยู่หรือไม่
    bool sawOpen = false; // เคยตรวจพบว่าประตูถูกเปิดใน session นี้แล้วหรือไม่
    bool lastDoorOpen = false; // สถานะเปิด/ปิดประตูล่าสุดที่จดจำไว้
    bool holdWarnSilenced = false; // ผู้ใช้กด silence ระหว่างเตือนประตูเปิดค้างแล้วหรือไม่
    uint32_t unlockDeadlineMs = 0; // เวลาหมดเขตหลังปลดล็อก
    uint32_t openWarnAtMs = 0; // เวลาที่เริ่มให้เตือนประตูเปิดค้าง
    uint32_t closeLockAtMs = 0; // เวลาที่ควรล็อกกลับหลังประตูปิด
    uint32_t nextWarnMs = 0; // เวลาถัดไปสำหรับเตือนซ้ำ
  };

  struct PendingDoorLockStatus {
    bool active = false; // มีสถานะ lock ที่ต้องรอส่งหลังเซอร์โวถึงตำแหน่งหรือไม่
    char reason[32] = {}; // reason ที่จะส่งขึ้น MQTT เมื่อเงื่อนไขพร้อม
  };

  HardwareAbstractionLayer::Actuators actuators_{}; // ตัวจัดการ actuator (servo + buzzer)
  CoreServices::MqttService mqtt_{}; // ตัวจัดการ MQTT/Wi-Fi และ publish queue
  CoreServices::NvsStorage nvs_{}; // ตัวจัดการบันทึกสถานะลง NVS

  ConfigurationSharedTypes::SystemState state_{}; // สถานะระบบหลักที่ทุกกฎใช้อ้างอิง
  DoorSession doorSession_{}; // สถานะย่อยของ workflow ปลดล็อกประตู
  PendingDoorLockStatus pendingDoorLockStatus_{}; // คิวสถานะ lock ที่ต้องรอเซอร์โวเข้าตำแหน่ง
  EventCollector* collector_ = nullptr; // ตัวอ่านอินพุตจากคีย์แพด/ปุ่ม/เซนเซอร์

  QueueHandle_t commandQueue_ = nullptr; // คิวรับคำสั่งรีโมตจาก MQTT callback
  uint32_t nextStatusAtMs_ = 0; // เวลาถัดไปที่ต้องส่ง periodic status
  ConfigurationSharedTypes::SystemState lastSerialState_{}; // state ล่าสุดที่ใช้เทียบ delta บน serial
  bool hasLastSerialState_ = false; // มี baseline สำหรับเทียบ serial delta แล้วหรือไม่

  static bool reached_(uint32_t nowMs, uint32_t targetMs); // เช็กเวลาแบบรองรับการ overflow ของ millis()
  static void copyText_(char* out, size_t outLen, const char* in); // คัดลอกข้อความลงบัฟเฟอร์แบบปลอดภัย (กันล้น)
  static String normalize_(String text); // trim + แปลงเป็นตัวพิมพ์เล็กเพื่อเทียบคำสั่ง
  static ConfigurationSharedTypes::Mode sanitizeBaseMode_(ConfigurationSharedTypes::Mode mode); // จำกัด base mode ให้เหลือ disarm/away เท่านั้น
  static void serialLogEvent_(const char* path, const ConfigurationSharedTypes::Event& event); // พิมพ์ event log แบบสั้นลง serial
  void serialLogStateChanges_(); // พิมพ์เฉพาะฟิลด์ state ที่เปลี่ยนและอัปเดต baseline

  void applyActiveModeFromBase_(); // คำนวณ active mode จาก latest_mode + is_night
  void persistModeState_(); // บันทึก latest_mode และ is_night ลง NVS

  bool pollRemoteCommand_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // อ่านและแปลคำสั่งจาก commandQueue เป็น event
  void syncSnapshot_(); // ซิงก์สถานะ open/locked จาก hardware ปัจจุบันเข้า state_

  void startDoorSession_(uint32_t nowMs, bool doorOpen); // เริ่ม session ประตูหลังปลดล็อกเพื่อรองรับเตือน/ล็อกกลับอัตโนมัติ
  void clearDoorSession_(bool silenceBuzzer); // เคลียร์ session ประตู และเลือกปิดเสียงเตือนได้
  void updateDoorSession_(uint32_t nowMs, bool doorOpen); // อัปเดตสถานะ session ประตูในแต่ละ tick

  void countdownView_(uint32_t nowMs,
                      bool& active,
                      uint32_t& deadlineMs,
                      uint32_t& warnBeforeMs) const; // คำนวณข้อมูลนับถอยหลังสำหรับแสดงผลบน OLED

  void triggerWarn_(const char* reason); // ยกระดับเป็น warn, สั่งบัซเซอร์ และส่ง reason ออก MQTT
  void enqueueStatus_(const char* reason); // เข้าคิวส่งข้อความสถานะไป MQTT
  void enqueueEvent_(const ConfigurationSharedTypes::Event& event, const char* flag); // เข้าคิวส่งข้อความอีเวนต์ไป MQTT
  void enqueueAck_(const char* command, bool ok, const char* detail); // เข้าคิวส่งผลตอบรับคำสั่ง (ack) ไป MQTT
};

} // namespace ApplicationLogicLayer
