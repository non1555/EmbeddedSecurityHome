#pragma once
#include <Arduino.h>

// Pin map for the separate "automation" ESP32 board.
// Goal: keep it simple and avoid ESP32 strapping pins (GPIO0/2/12/15) for outputs.
namespace AutoHw {

constexpr uint8_t PIN_UNUSED = 255;

// I2C (optional, if you later add BH1750/OLED/etc.)
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

// Outputs
constexpr uint8_t PIN_RELAY_LIGHT = 26;  // relay input (active HIGH)
constexpr uint8_t PIN_FAN_SWITCH  = 25;  // MOSFET/relay gate for fan (active HIGH)
constexpr uint8_t PIN_STATUS_LED  = 27;  // optional indicator LED (active HIGH)

// Sensors
constexpr uint8_t PIN_DHT = 32;          // DHT11/22 data pin

// Local light automation (BH1750 -> relay) thresholds.
// Turn ON when dark (< LUX_ON) and turn OFF when bright (> LUX_OFF).
constexpr float LUX_ON = 120.0f;
constexpr float LUX_OFF = 180.0f;
constexpr uint32_t LIGHT_SAMPLE_MS = 400;

} // namespace AutoHw
