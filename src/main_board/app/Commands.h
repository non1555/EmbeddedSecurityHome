#pragma once
#include <Arduino.h>

enum class CommandType {
  none,
  buzzer_warn,
  buzzer_alert,
  servo_lock
};

static const char* toString(CommandType t) {
  switch (t) {
    case CommandType::none:         return "none";
    case CommandType::buzzer_warn:  return "buzzer_warn";
    case CommandType::buzzer_alert: return "buzzer_alert";
    case CommandType::servo_lock:   return "servo_lock";
    default:                        return "unknown";
  }
}


struct Command {
  CommandType type;
  uint32_t ts_ms;
};
