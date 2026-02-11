#pragma once
#include <Arduino.h>

enum class EventType {
  disarm,
  arm_night,
  arm_away,
  window_open,
  vib_spike,
  motion,
  chokepoint
};

static const char* toString(EventType t) {
  switch (t) {
    case EventType::arm_night:   return "arm_night";
    case EventType::arm_away:    return "arm_away";
    case EventType::disarm:      return "disarm";
    case EventType::window_open: return "window_open";
    case EventType::vib_spike:   return "vib_spike";
    case EventType::motion:      return "motion";
    case EventType::chokepoint:  return "chokepoint";
    default:                     return "unknown";
  }
}

struct Event {
  EventType type = EventType::disarm;
  uint32_t ts_ms = 0;
  uint8_t src = 0;

  Event() = default;
  constexpr Event(EventType t, uint32_t ts, uint8_t s = 0)
  : type(t), ts_ms(ts), src(s) {}
};
