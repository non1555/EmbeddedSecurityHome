#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "configuration_shared_types/Types.h"

namespace CoreServices {

class MqttService {
public:
  void begin(QueueHandle_t commandQueue); // เริ่มต้น Wi-Fi/MQTT และผูกคิวรับคำสั่งรีโมต
  void loop(uint32_t nowMs); // รันรอบบริการ MQTT แบบไม่บล็อก (reconnect, poll, drain publish)

  bool enqueue(const ConfigurationSharedTypes::PublishMessage& msg); // เข้าคิวข้อความขาออกเพื่อส่ง MQTT แบบ asynchronous
  bool isConnected(); // คืนค่าสถานะว่า MQTT session เชื่อมต่ออยู่หรือไม่

private:
  static MqttService* self_; // pointer สำหรับใช้ใน callback แบบ static

  WiFiClient wifi_; // TCP client ของ Wi-Fi สำหรับ MQTT
  PubSubClient mqtt_{wifi_}; // MQTT client หลัก

  QueueHandle_t commandQueue_ = nullptr; // คิวรับคำสั่งจาก MQTT callback ไปยัง logic
  QueueHandle_t publishQueue_ = nullptr; // คิวข้อความที่ต้องส่งออก MQTT

  uint32_t nextWifiRetryMs_ = 0; // เวลาถัดไปที่อนุญาตให้ลอง reconnect Wi-Fi
  uint32_t nextMqttRetryMs_ = 0; // เวลาถัดไปที่อนุญาตให้ลอง reconnect MQTT
  uint32_t nextMetricsMs_ = 0; // เวลาถัดไปที่ต้องส่ง metrics
  uint32_t publishDrops_ = 0; // จำนวนข้อความขาออกที่ตกคิวส่ง
  uint32_t commandDrops_ = 0; // จำนวนคำสั่งขาเข้าที่ตกคิวรับ
  bool rawConnected_ = false; // สถานะ MQTT ดิบล่าสุด (ยังไม่ debounce)
  bool hasRawConnected_ = false; // เคยรับค่าสถานะดิบแล้วหรือไม่
  bool stableConnected_ = false; // สถานะ MQTT หลัง debounce
  bool hasStableConnected_ = false; // เคยกำหนดสถานะ stable แล้วหรือไม่
  uint32_t rawChangedAtMs_ = 0; // เวลาที่สถานะดิบเปลี่ยนล่าสุด

  void connectWifi_(uint32_t nowMs); // ลอง reconnect Wi-Fi ตามช่วงเวลาที่กำหนดแบบไม่บล็อก
  void connectMqtt_(uint32_t nowMs); // ลอง reconnect MQTT เมื่อ Wi-Fi พร้อมใช้งาน
  void drainPublishQueue_(); // ดึงข้อความจาก publishQueue ไปส่ง broker ตาม burst ที่กำหนด
  void updateConnectionSignal_(uint32_t nowMs); // ทำ debounce สถานะเชื่อมต่อ MQTT และอัปเดตไฟสถานะบนบอร์ด
  void setStatusLed_(bool connected); // ตั้งระดับไฟสถานะตาม polarity ที่กำหนดใน Config

  static void onMqttMessage_(char* topic, uint8_t* payload, unsigned int length); // callback เมื่อมีข้อความ MQTT เข้า แล้วส่งต่อเข้าคิวคำสั่ง
};

} // namespace CoreServices
