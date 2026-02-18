#pragma once
#include <Arduino.h>

namespace HwCfg {

constexpr uint8_t PIN_UNUSED = 255;

// ============================
// Actuators
// ============================
constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_SERVO1 = 26;
constexpr uint8_t PIN_SERVO2 = 27;

// ============================
// Perimeter Contacts (Reed)
// ============================
constexpr uint8_t PIN_REED_1 = 32; // ประตูหลัก (door reed)
constexpr uint8_t PIN_REED_2 = 19; // หน้าต่าง (window reed)

// ============================
// PIR
// ============================
constexpr uint8_t PIN_PIR_1  = 35; // โซนในบ้าน 1
constexpr uint8_t PIN_PIR_2  = 36; // โซนในบ้าน 2
constexpr uint8_t PIN_PIR_3  = 39; // โซนนอกบ้าน (outdoor PIR)

// ============================
// Vibration
// ============================
constexpr uint8_t PIN_VIB_1  = 34; // vibration รวม (ประตู/หน้าต่าง)

// ============================
// Ultrasonic
// ============================
constexpr uint8_t PIN_US_TRIG = 13;   // chokepoint #1: โซนประตู
constexpr uint8_t PIN_US_ECHO = 14;   // chokepoint #1: โซนประตู
constexpr uint8_t PIN_US_TRIG_2 = 16; // chokepoint #2: ทางผ่านระหว่างห้อง
constexpr uint8_t PIN_US_ECHO_2 = 17; // chokepoint #2: ทางผ่านระหว่างห้อง
constexpr uint8_t PIN_US_TRIG_3 = 4;  // chokepoint #3: ทางผ่านระหว่างห้อง
constexpr uint8_t PIN_US_ECHO_3 = 5;  // chokepoint #3: ทางผ่านระหว่างห้อง

constexpr uint8_t PIN_BTN_DOOR_TOGGLE = 33;
constexpr uint8_t PIN_BTN_WINDOW_TOGGLE = 18;

// ============================
// I2C Bus (Shared)
// ============================
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t KEYPAD_I2C_ADDR = 0x20;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;
constexpr uint8_t BH1750_I2C_ADDR = 0x23;

constexpr char KP_MAP[16] = {
  '1','2','3','A',
  '4','5','6','B',
  '7','8','9','C',
  '*','0','#','D'
};

// ============================
// Home Automation
// ============================
// Main board no longer owns home-automation IO.
// Use src/auto_board/hardware/AutoHardwareConfig.h on the automation board instead.
constexpr uint8_t PIN_STATUS_LED = PIN_UNUSED;
constexpr uint8_t PIN_RELAY_LIGHT = PIN_UNUSED;

constexpr uint8_t PIN_DHT = PIN_UNUSED;
constexpr uint8_t PIN_FAN = PIN_UNUSED;

constexpr uint8_t PIN_L293D_IN1 = PIN_UNUSED;
constexpr uint8_t PIN_L293D_IN2 = PIN_UNUSED;
constexpr uint8_t FAN_LEDC_CH = 3;
constexpr uint32_t FAN_PWM_HZ = 5000;
constexpr uint8_t FAN_PWM_RES_BITS = 8;
constexpr uint8_t FAN_PWM_DUTY_ON = 200;

constexpr float TEMP_ON_C = 30.0f;
constexpr float TEMP_OFF_C = 27.0f;
constexpr uint32_t TEMP_POLL_MS = 2000;

} // namespace HwCfg
