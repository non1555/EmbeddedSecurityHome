#pragma once
#include <Arduino.h>

enum class Mode { disarm, away, night };
enum class AlarmLevel { off, warn, alert, critical };

struct SystemState {
  Mode mode = Mode::disarm;
  AlarmLevel level = AlarmLevel::off;

  uint32_t last_notify_ms = 0;
  uint32_t last_indoor_activity_ms = 0;
  bool entry_pending = false;
  uint32_t entry_deadline_ms = 0;

  uint8_t suspicion_score = 0;
  uint32_t last_suspicion_update_ms = 0;
  uint32_t last_outdoor_motion_ms = 0;
  uint32_t last_window_event_ms = 0;
  uint32_t last_vibration_ms = 0;
  uint32_t last_door_event_ms = 0;

  bool keep_window_locked_when_disarmed = false;
};
