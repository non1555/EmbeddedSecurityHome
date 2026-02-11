#include "RuleEngine.h"

Decision RuleEngine::handle(const SystemState& s, const Config& cfg, const Event& e) const {
  Decision d{ s, {CommandType::none, e.ts_ms} };

  if (e.type == EventType::disarm) {
    d.next.mode = Mode::disarm;
    d.next.level = AlarmLevel::off;
    return d;
  }

  if (e.type == EventType::arm_night) {
    d.next.mode = Mode::night;
    d.next.level = AlarmLevel::off;
    return d;
  }

  if (s.mode == Mode::night && e.type == EventType::window_open) {
    d.next.level = AlarmLevel::alert;
    d.cmd.type = CommandType::buzzer_alert;
    return d;
  }

  if (s.mode == Mode::night &&
      (e.type == EventType::motion || e.type == EventType::chokepoint)) {
    if ((int)d.next.level < (int)AlarmLevel::warn) {
      d.next.level = AlarmLevel::warn;
    }
    d.cmd.type = CommandType::buzzer_warn;
    return d;
  }

  if (s.mode == Mode::night && e.type == EventType::vib_spike) {
    d.next.level = AlarmLevel::critical;
    // กันสแปม notify ง่าย ๆ
    if (e.ts_ms - s.last_notify_ms >= cfg.notify_cooldown_ms) {
      d.next.last_notify_ms = e.ts_ms;
      d.cmd.type = CommandType::notify;
    }
    return d;
  }

  return d;
}
