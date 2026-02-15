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
// Separate door/window contacts so lock checks can be accurate.
constexpr uint8_t PIN_REED_1 = 35; // door
constexpr uint8_t PIN_REED_2 = 32; // window

// ============================
// PIR
// ============================
constexpr uint8_t PIN_PIR_1  = 33;
constexpr uint8_t PIN_PIR_2  = 18;
constexpr uint8_t PIN_PIR_3  = 19;

// ============================
// Vibration
// ============================
// Wire vibration switches together into one input (series/parallel) on PIN_VIB_1.
constexpr uint8_t PIN_VIB_1  = 34;
constexpr uint8_t PIN_VIB_2  = PIN_UNUSED; // spare (disabled)

// ============================
// Ultrasonic
// ============================
constexpr uint8_t PIN_US_TRIG = 13;
constexpr uint8_t PIN_US_ECHO = 14;
constexpr uint8_t PIN_US_TRIG_2 = 16;
constexpr uint8_t PIN_US_ECHO_2 = 17;
constexpr uint8_t PIN_US_TRIG_3 = 4;
constexpr uint8_t PIN_US_ECHO_3 = 5;

// Active-low buttons (wire button to GND). Avoid holding during boot.
// Two-button setup: toggle door lock, toggle window lock.
constexpr uint8_t PIN_BTN_DOOR_TOGGLE = 15;
// Avoid GPIO2 (ESP32 strap pin). Use a safe GPIO for this button.
constexpr uint8_t PIN_BTN_WINDOW_TOGGLE = 23;

// ============================
// I2C Bus (Shared)
// ============================
// Shared by I2C keypad expander + SSD1306 OLED.
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t KEYPAD_I2C_ADDR = 0x20; // PCF8574 base address
constexpr uint8_t OLED_I2C_ADDR = 0x3C;   // SSD1306 default

// Key map for 4x4 keypad:
// - '#' submit
// - '*' or 'D' backspace (firmware)
// - 'C' clear (firmware)
constexpr char KP_MAP[16] = {
  '1','2','3','A',
  '4','5','6','B',
  '7','8','9','C',
  '*','0','#','D'
};

// ============================
// Home Automation (Optional)
// ============================
// These are disabled by default to avoid pin conflicts with the security build.
// Set them to real GPIO numbers if you actually wire these modules.

// LDR analog input pin (ADC1 recommended). Input-only pins are OK for LDR.
// Chosen: GPIO36 (VP) because it's input-only and you said it's free.
constexpr uint8_t PIN_LDR = 36;
// Relay output pin for light.
// Chosen: GPIO12 (STRAP PIN). If boot becomes flaky, move this to a truly free GPIO.
constexpr uint8_t PIN_RELAY_LIGHT = 12;
constexpr int LIGHT_ADC_THRESHOLD = 1500;

// DHT sensor pin (must be output-capable because DHT is bidirectional).
// Chosen: GPIO2 (STRAP PIN). Avoid holding this LOW during boot.
constexpr uint8_t PIN_DHT = 2;
// Fan PWM output pin.
// Chosen: GPIO0 (STRAP PIN). Avoid holding this LOW during boot.
constexpr uint8_t PIN_FAN = 0;
// Keep LEDC channels 0..2 reserved by buzzer/servos in this project.
constexpr uint8_t FAN_LEDC_CH = 3;
constexpr uint32_t FAN_PWM_HZ = 5000;
constexpr uint8_t FAN_PWM_RES_BITS = 8;
constexpr uint8_t FAN_PWM_DUTY_ON = 200; // ~78% for 8-bit

constexpr float TEMP_ON_C = 30.0f;
constexpr float TEMP_OFF_C = 27.0f;
constexpr uint32_t TEMP_POLL_MS = 2000;

} // namespace HwCfg
