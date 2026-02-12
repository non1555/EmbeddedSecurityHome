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

  if (e.type == EventType::arm_away) {
    d.next.mode = Mode::away;
    d.next.level = AlarmLevel::off;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::window_open) {
    d.next.level = AlarmLevel::alert;
    d.cmd.type = CommandType::buzzer_alert;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::door_tamper) {
    d.next.level = AlarmLevel::critical;
    d.next.last_notify_ms = e.ts_ms;
    d.cmd.type = CommandType::notify;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) &&
      (e.type == EventType::motion || e.type == EventType::chokepoint)) {
    AlarmLevel target = (s.mode == Mode::away) ? AlarmLevel::alert : AlarmLevel::warn;
    if ((int)d.next.level < (int)target) {
      d.next.level = target;
    }
    d.cmd.type = (s.mode == Mode::away) ? CommandType::buzzer_alert : CommandType::buzzer_warn;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::vib_spike) {
    d.next.level = AlarmLevel::critical;
    if (e.ts_ms - s.last_notify_ms >= cfg.notify_cooldown_ms) {
      d.next.last_notify_ms = e.ts_ms;
      d.cmd.type = CommandType::notify;
    }
    return d;
  }

  return d;
}
