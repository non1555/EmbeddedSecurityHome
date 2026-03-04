#pragma once

#include <Arduino.h>

namespace ConfigurationSharedTypes {
namespace Config {

constexpr uint8_t PIN_UNUSED = 255; // ค่าพินพิเศษสำหรับระบุว่า "ไม่ใช้งาน"

constexpr uint8_t PIN_BUZZER = 25; // พินควบคุมบัซเซอร์
constexpr uint8_t PIN_SERVO_DOOR = 26; // พินควบคุมเซอร์โวล็อกประตู
constexpr uint8_t PIN_SERVO_WINDOW = 27; // พินควบคุมเซอร์โวล็อกหน้าต่าง

constexpr uint8_t PIN_REED_DOOR = 32; // พินรับสัญญาณรีดสวิตช์ประตู
constexpr uint8_t PIN_REED_WINDOW = 19; // พินรับสัญญาณรีดสวิตช์หน้าต่าง

constexpr uint8_t PIN_PIR_1 = 35; // พินรับสัญญาณ PIR ตัวที่ 1
constexpr uint8_t PIN_PIR_2 = 36; // พินรับสัญญาณ PIR ตัวที่ 2
constexpr uint8_t PIN_PIR_3 = 39; // พินรับสัญญาณ PIR ตัวที่ 3

constexpr uint8_t PIN_VIB = 34; // พินรับสัญญาณเซนเซอร์สั่นสะเทือน
constexpr uint8_t PIN_BTN_DOOR_TOGGLE = 33; // พินปุ่มสลับล็อก/ปลดล็อกประตู
constexpr uint8_t PIN_BTN_WINDOW_TOGGLE = 18; // พินปุ่มสลับล็อก/ปลดล็อกหน้าต่าง

constexpr uint8_t PIN_US_TRIG_1 = 13; // พิน TRIG ของอัลตราโซนิกตัวที่ 1
constexpr uint8_t PIN_US_ECHO_1 = 14; // พิน ECHO ของอัลตราโซนิกตัวที่ 1
constexpr uint8_t PIN_US_TRIG_2 = 16; // พิน TRIG ของอัลตราโซนิกตัวที่ 2
constexpr uint8_t PIN_US_ECHO_2 = 17; // พิน ECHO ของอัลตราโซนิกตัวที่ 2
constexpr uint8_t PIN_US_TRIG_3 = 4; // พิน TRIG ของอัลตราโซนิกตัวที่ 3
constexpr uint8_t PIN_US_ECHO_3 = 5; // พิน ECHO ของอัลตราโซนิกตัวที่ 3
constexpr uint8_t PIN_STATUS_LED = 2; // พินไฟสถานะบนบอร์ด ESP32
constexpr bool STATUS_LED_ACTIVE_HIGH = true; // true=HIGH แล้วไฟติด, false=LOW แล้วไฟติด

constexpr uint8_t PIN_I2C_SDA = 21; // พิน SDA ของบัส I2C
constexpr uint8_t PIN_I2C_SCL = 22; // พิน SCL ของบัส I2C
constexpr uint8_t KEYPAD_I2C_ADDR = 0x20; // ที่อยู่ I2C ของ PCF8574 (คีย์แพด)
constexpr uint8_t OLED_I2C_ADDR = 0x3C; // ที่อยู่ I2C ของจอ OLED SSD1306

constexpr char KEYPAD_MAP[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
}; // ตารางแมปปุ่มคีย์แพด 4x4 เป็นตัวอักษรใช้งาน

constexpr uint32_t RTOS_TICK_MS = 20; // คาบลูปของ SecTask (มิลลิวินาที)
constexpr uint32_t STATUS_PERIOD_MS = 5000; // ช่วงเวลาส่งสถานะ periodic (มิลลิวินาที)
constexpr uint32_t MQTT_TASK_MS = 10; // คาบลูปของ MqttTask (มิลลิวินาที)
constexpr uint32_t MQTT_CONN_DEBOUNCE_MS = 1200; // เวลาดีบาวซ์สถานะ MQTT ก่อนอัปเดตไฟสถานะ

constexpr uint32_t ENTRY_DELAY_MS = 30000; // เวลา grace ก่อนยกระดับเมื่อเข้าโหมดเฝ้าระวัง (มิลลิวินาที)
constexpr uint32_t EXIT_STAGE_TIMEOUT_MS = 15000; // timeout ของแต่ละขั้น auto-arm (มิลลิวินาที)
constexpr uint32_t EXIT_AUTO_ARM_WINDOW_MS = 20000; // เวลารอในขั้นสุดท้ายก่อน arm away อัตโนมัติ (มิลลิวินาที)
constexpr uint32_t EXIT_GRACE_AFTER_INDOOR_MS = 30000; // เวลาผ่อนผันหลังเจอ indoor motion (มิลลิวินาที)

constexpr bool AUTO_ARM_ENABLED = true; // เปิด/ปิดฟังก์ชัน auto-arm

constexpr uint32_t REED_DEBOUNCE_MS = 80; // ดีบาวซ์รีดสวิตช์ (มิลลิวินาที)
constexpr uint32_t PIR_COOLDOWN_MS = 1500; // เวลาคูลดาวน์ PIR ต่ออีเวนต์ (มิลลิวินาที)
constexpr uint32_t VIB_COOLDOWN_MS = 700; // เวลาคูลดาวน์เซนเซอร์สั่น (มิลลิวินาที)
constexpr uint32_t US_SAMPLE_MS = 200; // คาบอ่านอัลตราโซนิกแบบสลับตัว (มิลลิวินาที)
constexpr uint32_t US_COOLDOWN_MS = 1500; // เวลาคูลดาวน์อัลตราโซนิก (มิลลิวินาที)
constexpr uint32_t US_ECHO_TIMEOUT_US = 6000; // timeout การอ่าน echo (ไมโครวินาที)
constexpr int US_NEAR_CM = 5; // ระยะที่ถือว่า "ใกล้มาก" (เซนติเมตร)
constexpr int US_FAR_CM = 8; // ระยะที่ถือว่า "พ้นโซนใกล้" (เซนติเมตร)

constexpr uint32_t DOOR_UNLOCK_TIMEOUT_MS = 15000; // เวลารอหลังปลดล็อกประตู (มิลลิวินาที)
constexpr uint32_t DOOR_UNLOCK_WARN_BEFORE_MS = 5000; // เวลาก่อน timeout ที่เริ่มเตือน (มิลลิวินาที)
constexpr uint32_t DOOR_OPEN_HOLD_WARN_AFTER_MS = 10000; // เวลาเปิดค้างก่อนเตือนซ้ำ (มิลลิวินาที)
constexpr uint32_t DOOR_WARN_RETRIGGER_MS = 350; // คาบเตือนซ้ำระหว่างสถานะเตือน (มิลลิวินาที)
constexpr uint32_t DOOR_AUTOLOCK_AFTER_CLOSE_MS = 3000; // หน่วงก่อนล็อกกลับอัตโนมัติหลังปิดประตู (มิลลิวินาที)

constexpr uint8_t DOOR_LOCK_DEG = 10; // มุมเซอร์โวเมื่ออยู่สถานะล็อกประตู
constexpr uint8_t DOOR_UNLOCK_DEG = 90; // มุมเซอร์โวเมื่ออยู่สถานะปลดล็อกประตู
constexpr uint8_t WINDOW_LOCK_DEG = 10; // มุมเซอร์โวเมื่ออยู่สถานะล็อกหน้าต่าง
constexpr uint8_t WINDOW_UNLOCK_DEG = 90; // มุมเซอร์โวเมื่ออยู่สถานะปลดล็อกหน้าต่าง

} // namespace Config
} // namespace ConfigurationSharedTypes
