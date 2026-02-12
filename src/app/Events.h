#pragma once
#include <Arduino.h>

enum class EventType {
  disarm,
  arm_night,
  arm_away,
  door_open,
  window_open,
  door_tamper,
  vib_spike,
  motion,
  chokepoint,
  door_hold_warn_silence,
  manual_lock_request,
  manual_unlock_request,
  entry_timeout
};

static const char* toString(EventType t) {
  switch (t) {
    case EventType::arm_night:   return "arm_night";
    case EventType::arm_away:    return "arm_away";
    case EventType::disarm:      return "disarm";
    case EventType::door_open:   return "door_open";
    case EventType::window_open: return "window_open";
    case EventType::door_tamper: return "door_tamper";
    case EventType::vib_spike:   return "vib_spike";
    case EventType::motion:      return "motion";
    case EventType::chokepoint:  return "chokepoint";
    case EventType::door_hold_warn_silence: return "door_hold_warn_silence";
    case EventType::manual_lock_request: return "manual_lock_request";
    case EventType::manual_unlock_request: return "manual_unlock_request";
    case EventType::entry_timeout:return "entry_timeout";
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
