#include "RuleEngine.h"

namespace {
static inline bool within(uint32_t nowMs, uint32_t refMs, uint32_t windowMs) {
  return refMs != 0 && (nowMs - refMs) <= windowMs;
}

static AlarmLevel levelFromScore(uint8_t score) {
  if (score >= 80) return AlarmLevel::critical;
  if (score >= 45) return AlarmLevel::alert;
  if (score >= 15) return AlarmLevel::warn;
  return AlarmLevel::off;
}

static void applyDecay(SystemState& st, const Config& cfg, uint32_t nowMs) {
  if (st.last_suspicion_update_ms == 0) {
    st.last_suspicion_update_ms = nowMs;
    return;
  }
  if (cfg.suspicion_decay_step_ms == 0 || cfg.suspicion_decay_points == 0) {
    st.last_suspicion_update_ms = nowMs;
    return;
  }

  const uint32_t elapsed = nowMs - st.last_suspicion_update_ms;
  const uint32_t steps = elapsed / cfg.suspicion_decay_step_ms;
  if (steps == 0) return;

  const uint32_t decay = steps * cfg.suspicion_decay_points;
  st.suspicion_score = (decay >= st.suspicion_score) ? 0 : (uint8_t)(st.suspicion_score - decay);
  st.last_suspicion_update_ms = nowMs;
}

static void addScore(SystemState& st, uint8_t points) {
  const uint16_t s = (uint16_t)st.suspicion_score + points;
  st.suspicion_score = (s > 100) ? 100 : (uint8_t)s;
}
} // namespace

Decision RuleEngine::handle(const SystemState& s, const Config& cfg, const Event& e) const {
  Decision d{ s, {CommandType::none, e.ts_ms} };
  applyDecay(d.next, cfg, e.ts_ms);

  if (e.type == EventType::disarm) {
    d.next.mode = Mode::disarm;
    d.next.level = AlarmLevel::off;
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.suspicion_score = 0;
    d.next.last_suspicion_update_ms = e.ts_ms;
    d.next.last_outdoor_motion_ms = 0;
    d.next.last_window_event_ms = 0;
    d.next.last_vibration_ms = 0;
    d.next.last_door_event_ms = 0;
    d.next.keep_window_locked_when_disarmed = false;
    return d;
  }

  if (e.type == EventType::arm_night) {
    d.next.mode = Mode::night;
    d.next.level = AlarmLevel::off;
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.suspicion_score = 0;
    d.next.last_suspicion_update_ms = e.ts_ms;
    d.next.last_outdoor_motion_ms = 0;
    d.next.last_window_event_ms = 0;
    d.next.last_vibration_ms = 0;
    d.next.last_door_event_ms = 0;
    d.next.keep_window_locked_when_disarmed = false;
    return d;
  }

  if (e.type == EventType::arm_away) {
    d.next.mode = Mode::away;
    d.next.level = AlarmLevel::off;
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.suspicion_score = 0;
    d.next.last_suspicion_update_ms = e.ts_ms;
    d.next.last_outdoor_motion_ms = 0;
    d.next.last_window_event_ms = 0;
    d.next.last_vibration_ms = 0;
    d.next.last_door_event_ms = 0;
    d.next.keep_window_locked_when_disarmed = false;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::door_open) {
    // Don't keep extending entry delay / stacking score if the door stays open or chatters.
    if (s.entry_pending) return d;
    if (s.last_indoor_activity_ms != 0 &&
        (e.ts_ms - s.last_indoor_activity_ms) <= cfg.exit_grace_after_indoor_activity_ms) {
      return d;
    }
    d.next.entry_pending = true;
    d.next.entry_deadline_ms = e.ts_ms + cfg.entry_delay_ms;
    d.next.last_door_event_ms = e.ts_ms;
    addScore(d.next, 15);
    d.next.level = levelFromScore(d.next.suspicion_score);
    d.cmd.type = CommandType::buzzer_warn;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::entry_timeout) {
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.suspicion_score = 100;
    d.next.level = AlarmLevel::critical;
    if (e.ts_ms - s.last_notify_ms >= cfg.notify_cooldown_ms) {
      d.next.last_notify_ms = e.ts_ms;
      d.cmd.type = CommandType::notify;
    } else {
      d.cmd.type = CommandType::buzzer_alert;
    }
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::window_open) {
    d.next.last_window_event_ms = e.ts_ms;
    addScore(d.next, 40);
    if (within(e.ts_ms, s.last_outdoor_motion_ms, cfg.correlation_window_ms)) addScore(d.next, 15);
    if (within(e.ts_ms, s.last_vibration_ms, cfg.correlation_window_ms)) addScore(d.next, 10);
    d.next.level = levelFromScore(d.next.suspicion_score);
    d.cmd.type = (d.next.level >= AlarmLevel::alert) ? CommandType::buzzer_alert : CommandType::buzzer_warn;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) &&
      (e.type == EventType::motion || e.type == EventType::chokepoint)) {
    const bool isIndoorActivity =
      (e.type == EventType::chokepoint) ||
      (e.type == EventType::motion && e.src != cfg.outdoor_pir_src);
    if (isIndoorActivity) {
      d.next.last_indoor_activity_ms = e.ts_ms;
      addScore(d.next, 18);
      if (within(e.ts_ms, s.last_window_event_ms, cfg.correlation_window_ms)) addScore(d.next, 20);
      if (within(e.ts_ms, s.last_vibration_ms, cfg.correlation_window_ms)) addScore(d.next, 12);
      if (within(e.ts_ms, s.last_door_event_ms, cfg.correlation_window_ms)) addScore(d.next, 8);
    } else {
      d.next.last_outdoor_motion_ms = e.ts_ms;
      addScore(d.next, 10);
    }
    d.next.level = levelFromScore(d.next.suspicion_score);
    d.cmd.type = (d.next.level >= AlarmLevel::alert) ? CommandType::buzzer_alert : CommandType::buzzer_warn;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::vib_spike) {
    d.next.last_vibration_ms = e.ts_ms;
    addScore(d.next, 22);
    if (within(e.ts_ms, s.last_outdoor_motion_ms, cfg.correlation_window_ms)) addScore(d.next, 12);
    if (within(e.ts_ms, s.last_window_event_ms, cfg.correlation_window_ms)) addScore(d.next, 10);
    d.next.level = levelFromScore(d.next.suspicion_score);

    if (d.next.level == AlarmLevel::critical) {
      d.next.entry_pending = false;
      d.next.entry_deadline_ms = 0;
      if (e.ts_ms - s.last_notify_ms >= cfg.notify_cooldown_ms) {
        d.next.last_notify_ms = e.ts_ms;
        d.cmd.type = CommandType::notify;
      } else {
        d.cmd.type = CommandType::buzzer_alert;
      }
      return d;
    }
    d.cmd.type = (d.next.level >= AlarmLevel::alert) ? CommandType::buzzer_alert : CommandType::buzzer_warn;
    return d;
  }

  if ((s.mode == Mode::night || s.mode == Mode::away) && e.type == EventType::door_tamper) {
    addScore(d.next, 65);
    if (within(e.ts_ms, s.last_outdoor_motion_ms, cfg.correlation_window_ms)) addScore(d.next, 15);
    d.next.level = levelFromScore(d.next.suspicion_score);
    if (d.next.level == AlarmLevel::critical && e.ts_ms - s.last_notify_ms >= cfg.notify_cooldown_ms) {
      d.next.last_notify_ms = e.ts_ms;
      d.cmd.type = CommandType::notify;
    } else {
      d.cmd.type = CommandType::buzzer_alert;
    }
    return d;
  }

  d.next.level = levelFromScore(d.next.suspicion_score);
  return d;
}
