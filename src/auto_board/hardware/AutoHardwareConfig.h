#pragma once
#include <Arduino.h>

// Pin map for the separate "automation" ESP32 board.
// Goal: keep it simple and avoid ESP32 strapping pins (GPIO0/2/12/15) for outputs.
namespace AutoHw {

constexpr uint8_t PIN_UNUSED = 255;

// I2C (optional, if you later add BH1750/OLED/etc.)
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t BH1750_ADDR_PRIMARY = 0x23;
constexpr uint8_t BH1750_ADDR_SECONDARY = 0x5C;

// Outputs
constexpr uint8_t PIN_LIGHT_LED   = 27;         // light indicator LED (active HIGH)
constexpr uint8_t PIN_L293D_IN1   = 25;         // fan motor via L293D IN1
constexpr uint8_t PIN_L293D_IN2   = 26;         // fan motor via L293D IN2
// Optional EN pin for L293D (set PIN_UNUSED if EN is tied HIGH in hardware).
constexpr uint8_t PIN_L293D_EN    = 33;         // firmware drives HIGH to enable fan channel

// Sensors
constexpr uint8_t PIN_DHT = 32;          // DHT11/22 data pin

// Local light automation (BH1750 -> LED) thresholds.
// Turn ON when dark (< LUX_ON) and turn OFF when bright (> LUX_OFF).
constexpr float LUX_ON = 120.0f;
constexpr float LUX_OFF = 180.0f;
constexpr uint32_t LIGHT_SAMPLE_MS = 400;

// Temperature fan automation thresholds (hysteresis).
constexpr float FAN_ON_C = 20.0f;
constexpr float FAN_OFF_C = 15.0f;
constexpr uint32_t TEMP_SAMPLE_MS = 2000;

} // namespace AutoHw
