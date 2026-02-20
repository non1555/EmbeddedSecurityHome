#pragma once
#include <Arduino.h>

#ifndef ALLOW_SERIAL_SENSOR_COMMANDS_DEFAULT
#define ALLOW_SERIAL_SENSOR_COMMANDS_DEFAULT 0
#endif

struct Config {
  uint32_t notify_cooldown_ms = 3000;
  uint32_t entry_delay_ms = 15000;
  uint32_t exit_grace_after_indoor_activity_ms = 30000;
  uint8_t outdoor_pir_src = 3;
  uint32_t correlation_window_ms = 20000;
  uint32_t suspicion_decay_step_ms = 5000;
  uint8_t suspicion_decay_points = 8;
  uint8_t door_ultrasonic_src = 1;
  bool allow_remote_without_token = false;
  bool require_remote_nonce = true;
  bool require_remote_monotonic_nonce = true;
  uint32_t remote_nonce_ttl_ms = 180000;
  bool fail_closed_if_nonce_persistence_unavailable = true;
  bool allow_serial_mode_commands = false;
  bool allow_serial_manual_commands = false;
  bool allow_serial_sensor_commands = (ALLOW_SERIAL_SENSOR_COMMANDS_DEFAULT != 0);
  bool serial_notify_enabled = false;
  bool sensor_health_enabled = true;
  uint32_t sensor_health_check_period_ms = 2000;
  uint32_t pir_stuck_active_ms = 180000;
  uint32_t vib_stuck_active_ms = 15000;
  uint32_t ultrasonic_offline_ms = 30000;
  uint16_t ultrasonic_no_echo_threshold = 80;
  uint32_t sensor_fault_notify_cooldown_ms = 60000;
  bool fail_closed_on_sensor_fault = true;

  // Door auto-relock session after keypad disarm.
  uint32_t door_unlock_timeout_ms = 15000;
  uint32_t door_unlock_warn_before_ms = 5000;
  uint32_t door_open_hold_warn_after_ms = 10000;
  uint32_t door_warn_retrigger_ms = 350;

  uint8_t keypad_bad_attempt_limit = 3;
  uint32_t keypad_lockout_ms = 300000;
};
