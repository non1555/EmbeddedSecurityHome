#pragma once
#include <Arduino.h>

// Set to 1 when keypad matrix is wired through an I2C expander.
// In this mode, on-board GPIO row/col scanning is disabled.
#ifndef KEYPAD_USE_I2C_EXPANDER
#define KEYPAD_USE_I2C_EXPANDER 1
#endif

namespace HwCfg {

constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_SERVO1 = 26;
constexpr uint8_t PIN_SERVO2 = 27;

constexpr uint8_t PIN_REED_1 = 35;
constexpr uint8_t PIN_PIR_1  = 33;
constexpr uint8_t PIN_VIB_1  = 34;

constexpr uint8_t PIN_US_TRIG = 13;
constexpr uint8_t PIN_US_ECHO = 14;

// ESP32 default I2C pins.
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t KEYPAD_I2C_ADDR = 0x20; // PCF8574 base address

constexpr char KP_MAP[16] = {
  '1','2','3','A',
  '4','5','6','B',
  '7','8','9','C',
  '*','0','#','D'
};

#if !KEYPAD_USE_I2C_EXPANDER
constexpr uint8_t KP_ROWS[4] = {16, 17, 18, 19};
constexpr uint8_t KP_COLS[4] = {21, 22, 23, 32};
#endif

} // namespace HwCfg
