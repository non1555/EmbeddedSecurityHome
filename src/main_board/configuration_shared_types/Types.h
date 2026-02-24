#pragma once

#include <Arduino.h>

namespace ConfigurationSharedTypes {

enum class Mode : uint8_t {
  disarm,
  away,
  night
};

enum class AlarmLevel : uint8_t {
  off,
  warn,
  alert
};

enum class EventType : uint8_t {
  disarm,
  arm_away,
  arm_night,
  door_open,
  window_open,
  door_tamper,
  vib_spike,
  motion,
  chokepoint,
  door_hold_warn_silence,
  keypad_help_request,
  door_code_unlock,
  door_code_bad,
  entry_timeout
};

enum class CommandType : uint8_t {
  none,
  buzzer_warn,
  buzzer_alert,
  buzzer_stop
};

struct Event {
  EventType type = EventType::disarm;
  uint32_t ts_ms = 0;
  uint8_t src = 0;

  constexpr Event() = default; // Creates default event (disarm, ts=0, src=0). Params: none.
  constexpr Event(EventType t, uint32_t ts, uint8_t s = 0)
  : type(t), ts_ms(ts), src(s) {} // Creates event with explicit type/time/source. Params: t=event type, ts=timestamp in ms, s=source id.
};

struct SystemState {
  Mode mode = Mode::disarm;
  Mode latest_mode = Mode::disarm;
  bool is_night = false;
  AlarmLevel level = AlarmLevel::off;

  bool entry_pending = false;
  uint32_t entry_deadline_ms = 0;
  uint32_t last_indoor_activity_ms = 0;
  uint32_t last_vibration_ms = 0;

  uint8_t failed_attempts = 0;

  uint8_t exit_stage = 0;
  uint32_t exit_timeout_ms = 0;

  bool door_locked = false;
  bool window_locked = false;
  bool door_open = false;
  bool window_open = false;
};

struct Decision {
  SystemState next{};
  CommandType cmd = CommandType::none;
  const char* flag = "";
};

enum class PublishKind : uint8_t {
  event,
  status,
  ack
};

struct PublishMessage {
  PublishKind kind = PublishKind::event;
  Event event{};
  SystemState st{};
  bool ok = false;
  char text1[32]{};
  char text2[64]{};
};

struct RemoteCommandMessage {
  char payload[128]{};
};

inline const char* toString(Mode m) { // Converts Mode enum to stable text token. Params: m=mode enum value.
  switch (m) {
    case Mode::disarm: return "disarm";
    case Mode::away: return "away";
    case Mode::night: return "night";
    default: return "unknown";
  }
}

inline const char* toString(AlarmLevel lv) { // Converts AlarmLevel enum to stable text token. Params: lv=alarm level enum value.
  switch (lv) {
    case AlarmLevel::off: return "off";
    case AlarmLevel::warn: return "warn";
    case AlarmLevel::alert: return "alert";
    default: return "unknown";
  }
}

inline const char* toString(EventType t) { // Converts EventType enum to stable text token. Params: t=event type enum value.
  switch (t) {
    case EventType::disarm: return "disarm";
    case EventType::arm_away: return "arm_away";
    case EventType::arm_night: return "arm_night";
    case EventType::door_open: return "door_open";
    case EventType::window_open: return "window_open";
    case EventType::door_tamper: return "door_tamper";
    case EventType::vib_spike: return "vib_spike";
    case EventType::motion: return "motion";
    case EventType::chokepoint: return "chokepoint";
    case EventType::door_hold_warn_silence: return "door_hold_warn_silence";
    case EventType::keypad_help_request: return "keypad_help_request";
    case EventType::door_code_unlock: return "door_code_unlock";
    case EventType::door_code_bad: return "door_code_bad";
    case EventType::entry_timeout: return "entry_timeout";
    default: return "unknown";
  }
}

inline const char* toString(CommandType t) { // Converts CommandType enum to stable text token. Params: t=command type enum value.
  switch (t) {
    case CommandType::none: return "none";
    case CommandType::buzzer_warn: return "buzzer_warn";
    case CommandType::buzzer_alert: return "buzzer_alert";
    case CommandType::buzzer_stop: return "buzzer_stop";
    default: return "unknown";
  }
}

} // namespace ConfigurationSharedTypes
