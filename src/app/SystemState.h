#pragma once
#include <Arduino.h>

enum class Mode { disarm, away, night };
enum class AlarmLevel { off, warn, alert, critical };

struct SystemState {
  Mode mode = Mode::disarm;
  AlarmLevel level = AlarmLevel::off;

  uint32_t last_notify_ms = 0; // กันสแปมเบื้องต้น
};
