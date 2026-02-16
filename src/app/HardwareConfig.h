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
constexpr uint8_t PIN_REED_1 = 32; // door
constexpr uint8_t PIN_REED_2 = 19; // window

// ============================
// PIR
// ============================
constexpr uint8_t PIN_PIR_1  = 35;
constexpr uint8_t PIN_PIR_2  = 36;
constexpr uint8_t PIN_PIR_3  = 39;

// ============================
// Vibration
// ============================
constexpr uint8_t PIN_VIB_1  = 34;

// ============================
// Ultrasonic
// ============================
constexpr uint8_t PIN_US_TRIG = 13;
constexpr uint8_t PIN_US_ECHO = 14;
constexpr uint8_t PIN_US_TRIG_2 = 16;
constexpr uint8_t PIN_US_ECHO_2 = 17;
constexpr uint8_t PIN_US_TRIG_3 = 4;
constexpr uint8_t PIN_US_ECHO_3 = 5;

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
constexpr uint8_t LIGHT_SENSOR_ADC_PIN = PIN_UNUSED;
constexpr uint8_t PIN_STATUS_LED = 2;
constexpr uint8_t PIN_RELAY_LIGHT = PIN_STATUS_LED;
constexpr int LIGHT_ADC_THRESHOLD = 1500;

constexpr uint8_t PIN_DHT = 15;
constexpr uint8_t PIN_FAN = 0;

constexpr uint8_t PIN_L293D_IN1 = 12;
constexpr uint8_t PIN_L293D_IN2 = 23;
constexpr uint8_t FAN_LEDC_CH = 3;
constexpr uint32_t FAN_PWM_HZ = 5000;
constexpr uint8_t FAN_PWM_RES_BITS = 8;
constexpr uint8_t FAN_PWM_DUTY_ON = 200;

constexpr float TEMP_ON_C = 30.0f;
constexpr float TEMP_OFF_C = 27.0f;
constexpr uint32_t TEMP_POLL_MS = 2000;

} // namespace HwCfg
