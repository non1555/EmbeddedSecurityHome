#pragma once

#include <Arduino.h>

namespace ConfigurationSharedTypes {
namespace Config {

constexpr uint8_t PIN_UNUSED = 255;

constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_SERVO_DOOR = 26;
constexpr uint8_t PIN_SERVO_WINDOW = 27;

constexpr uint8_t PIN_REED_DOOR = 32;
constexpr uint8_t PIN_REED_WINDOW = 19;

constexpr uint8_t PIN_PIR_1 = 35;
constexpr uint8_t PIN_PIR_2 = 36;
constexpr uint8_t PIN_PIR_3 = 39;

constexpr uint8_t PIN_VIB = 34;
constexpr uint8_t PIN_BTN_DOOR_TOGGLE = 33;
constexpr uint8_t PIN_BTN_WINDOW_TOGGLE = 18;

constexpr uint8_t PIN_US_TRIG_1 = 13;
constexpr uint8_t PIN_US_ECHO_1 = 14;
constexpr uint8_t PIN_US_TRIG_2 = 16;
constexpr uint8_t PIN_US_ECHO_2 = 17;
constexpr uint8_t PIN_US_TRIG_3 = 4;
constexpr uint8_t PIN_US_ECHO_3 = 5;

constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t KEYPAD_I2C_ADDR = 0x20;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;

constexpr char KEYPAD_MAP[16] = {
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};

constexpr uint32_t RTOS_TICK_MS = 20;
constexpr uint32_t STATUS_PERIOD_MS = 5000;
constexpr uint32_t MQTT_TASK_MS = 10;

constexpr uint32_t ENTRY_DELAY_MS = 30000;
constexpr uint32_t EXIT_STAGE_TIMEOUT_MS = 15000;
constexpr uint32_t EXIT_AUTO_ARM_WINDOW_MS = 20000;
constexpr uint32_t EXIT_GRACE_AFTER_INDOOR_MS = 30000;
constexpr uint32_t FORCED_ENTRY_WINDOW_MS = 3000;

constexpr bool AUTO_ARM_ENABLED = true;

constexpr uint32_t REED_DEBOUNCE_MS = 80;
constexpr uint32_t PIR_COOLDOWN_MS = 1500;
constexpr uint32_t VIB_COOLDOWN_MS = 700;
constexpr uint32_t US_SAMPLE_MS = 200;
constexpr uint32_t US_COOLDOWN_MS = 1500;
constexpr uint32_t US_ECHO_TIMEOUT_US = 6000;
constexpr int US_NEAR_CM = 5;
constexpr int US_FAR_CM = 8;

constexpr uint32_t DOOR_UNLOCK_TIMEOUT_MS = 15000;
constexpr uint32_t DOOR_UNLOCK_WARN_BEFORE_MS = 5000;
constexpr uint32_t DOOR_OPEN_HOLD_WARN_AFTER_MS = 10000;
constexpr uint32_t DOOR_WARN_RETRIGGER_MS = 350;
constexpr uint32_t DOOR_AUTOLOCK_AFTER_CLOSE_MS = 3000;

constexpr uint8_t DOOR_LOCK_DEG = 10;
constexpr uint8_t DOOR_UNLOCK_DEG = 90;
constexpr uint8_t WINDOW_LOCK_DEG = 10;
constexpr uint8_t WINDOW_UNLOCK_DEG = 90;

} // namespace Config
} // namespace ConfigurationSharedTypes
