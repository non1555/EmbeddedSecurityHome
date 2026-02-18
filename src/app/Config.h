#pragma once
#include <Arduino.h>

struct Config {
  uint32_t notify_cooldown_ms = 3000;
  uint32_t entry_delay_ms = 15000;
  uint32_t exit_grace_after_indoor_activity_ms = 30000;
  uint8_t outdoor_pir_src = 3;
  uint32_t correlation_window_ms = 20000;
  uint32_t suspicion_decay_step_ms = 5000;
  uint8_t suspicion_decay_points = 8;
  uint8_t door_ultrasonic_src = 1;

  // Door auto-relock session after keypad disarm.
  uint32_t door_unlock_timeout_ms = 15000;
  uint32_t door_unlock_warn_before_ms = 5000;
  uint32_t door_open_hold_warn_after_ms = 10000;
  uint32_t door_warn_retrigger_ms = 350;
};
