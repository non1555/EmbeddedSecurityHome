#include "application_logic_layer/RuleEngine.h"

#include "configuration_shared_types/Config.h"
#include "application_logic_layer/SystemContext.h"

namespace {

bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

bool isMotion(const ConfigurationSharedTypes::Event& e, uint8_t src) {
  return e.type == ConfigurationSharedTypes::EventType::motion && e.src == src;
}

bool isChokepoint(const ConfigurationSharedTypes::Event& e, uint8_t src) {
  return e.type == ConfigurationSharedTypes::EventType::chokepoint && e.src == src;
}

bool isAutoArmCancelEvent(const ConfigurationSharedTypes::Event& e) {
  return isMotion(e, 1) ||
         isMotion(e, 2) ||
         isMotion(e, 3) ||
         isChokepoint(e, 2) ||
         isChokepoint(e, 3);
}

bool isNightPerimeterBreach(const ConfigurationSharedTypes::Event& e) {
  return e.type == ConfigurationSharedTypes::EventType::door_open ||
         e.type == ConfigurationSharedTypes::EventType::window_open ||
         e.type == ConfigurationSharedTypes::EventType::vib_spike ||
         isMotion(e, 3);
}

void resetCommonForModeChange(ConfigurationSharedTypes::SystemState& st) {
  st.level = ConfigurationSharedTypes::AlarmLevel::off;
  st.exit_stage = 0;
  st.exit_timeout_ms = 0;
  st.failed_attempts = 0;
}

void setBaseMode(ConfigurationSharedTypes::SystemState& st,
                 ConfigurationSharedTypes::Mode baseMode) {
  st.latest_mode = (baseMode == ConfigurationSharedTypes::Mode::away)
    ? ConfigurationSharedTypes::Mode::away
    : ConfigurationSharedTypes::Mode::disarm;
  st.is_night = false;
  st.mode = st.latest_mode;
  resetCommonForModeChange(st);
}

void setNightMode(ConfigurationSharedTypes::SystemState& st) {
  if (st.latest_mode != ConfigurationSharedTypes::Mode::away &&
      st.latest_mode != ConfigurationSharedTypes::Mode::disarm) {
    st.latest_mode = ConfigurationSharedTypes::Mode::disarm;
  }
  st.is_night = true;
  st.mode = ConfigurationSharedTypes::Mode::night;
  resetCommonForModeChange(st);
}

} // namespace

namespace ApplicationLogicLayer {

void RuleEngine::process(const ConfigurationSharedTypes::Event& e, SystemContext& context) const {
  using namespace ConfigurationSharedTypes;

  if (e.type == EventType::door_hold_warn_silence) {
    context.handleSilenceRequest();
    return;
  }

  if (e.type == EventType::keypad_help_request) {
    context.handleHelpRequest(e);
    return;
  }

  if (e.type == EventType::manual_door_toggle ||
      e.type == EventType::manual_window_toggle) {
    context.handleManualToggle(e);
    return;
  }

  const Decision decision = handle(context.state(), e);
  context.applyDecision(e, decision);
}

ConfigurationSharedTypes::Decision RuleEngine::handle(const ConfigurationSharedTypes::SystemState& st,
                                                       const ConfigurationSharedTypes::Event& e) const {
  using namespace ConfigurationSharedTypes;

  Decision decision{};
  decision.next = st;
  decision.cmd = CommandType::none;
  decision.flag = "";

  if (e.type == EventType::disarm) {
    setBaseMode(decision.next, Mode::disarm);
    decision.flag = "mode_disarm";
    return decision;
  }

  if (e.type == EventType::arm_away) {
    setBaseMode(decision.next, Mode::away);
    decision.flag = "mode_away";
    return decision;
  }

  if (e.type == EventType::arm_night) {
    setNightMode(decision.next);
    decision.flag = "mode_night";
    return decision;
  }

  if (e.type == EventType::door_code_unlock) {
    setBaseMode(decision.next, Mode::disarm);
    decision.flag = "mode_disarm";
    return decision;
  }

  if (e.type == EventType::door_code_bad) {
    if (decision.next.failed_attempts < 0xFFu) {
      ++decision.next.failed_attempts;
    }

    if (decision.next.failed_attempts >= 3) {
      decision.next.level = AlarmLevel::alert;
      decision.cmd = CommandType::buzzer_alert;
      decision.flag = "keypad_alert";
    } else {
      decision.next.level = AlarmLevel::warn;
      decision.cmd = CommandType::buzzer_warn;
      decision.flag = "wrong_code";
    }

    return decision;
  }

  if (e.type == EventType::vib_spike) {
    decision.next.level = AlarmLevel::alert;
    decision.cmd = CommandType::buzzer_alert;
    decision.flag = (st.mode == Mode::night) ? "alert_night_breach" : "alert_high";
    return decision;
  }

  if (e.type == EventType::door_open && st.door_locked) {
    decision.next.level = AlarmLevel::alert;
    decision.cmd = CommandType::buzzer_alert;
    decision.flag = "alert_door";
    return decision;
  }

  if (e.type == EventType::window_open && st.window_locked) {
    decision.next.level = AlarmLevel::alert;
    decision.cmd = CommandType::buzzer_alert;
    decision.flag = (st.mode == Mode::night) ? "alert_night_breach" : "alert_high";
    return decision;
  }

  if (st.mode == Mode::disarm && Config::AUTO_ARM_ENABLED) {
    if (st.exit_stage == 0 && isChokepoint(e, 1)) {
      decision.next.exit_stage = 1;
      decision.next.exit_timeout_ms = e.ts_ms + Config::EXIT_STAGE_TIMEOUT_MS;
      decision.flag = "exit_stage_1";
      return decision;
    }

    if (st.exit_stage == 1 && e.type == EventType::door_open) {
      decision.next.exit_stage = 2;
      decision.next.exit_timeout_ms = e.ts_ms + Config::EXIT_STAGE_TIMEOUT_MS;
      decision.flag = "exit_stage_2";
      return decision;
    }

    if (st.exit_stage == 3 && isAutoArmCancelEvent(e)) {
      decision.next.exit_stage = 0;
      decision.next.exit_timeout_ms = 0;
      decision.flag = "auto_arm_cancel";
      return decision;
    }

    return decision;
  }

  if (st.mode == Mode::night) {
    if (isNightPerimeterBreach(e)) {
      decision.next.level = AlarmLevel::alert;
      decision.cmd = CommandType::buzzer_alert;
      decision.flag = "alert_night_breach";
    }
    return decision;
  }

  if (st.mode != Mode::away) {
    return decision;
  }

  if (e.type == EventType::window_open) {
    decision.next.level = AlarmLevel::alert;
    decision.cmd = CommandType::buzzer_alert;
    decision.flag = "alert_high";
    return decision;
  }

  if (isMotion(e, 3)) {
    decision.next.level = AlarmLevel::warn;
    decision.cmd = CommandType::buzzer_warn;
    decision.flag = "warn_outside_motion";
    return decision;
  }

  if (isMotion(e, 1) ||
      isMotion(e, 2) ||
      isChokepoint(e, 2) ||
      isChokepoint(e, 3)) {
    decision.next.level = AlarmLevel::alert;
    decision.cmd = CommandType::buzzer_alert;
    decision.flag = "alert_high";
    return decision;
  }

  if (e.type == EventType::door_open) {
    decision.next.level = AlarmLevel::alert;
    decision.cmd = CommandType::buzzer_alert;
    decision.flag = "alert_door";
    return decision;
  }

  return decision;
}

bool RuleEngine::processDisarmAutoArmTick(ConfigurationSharedTypes::SystemState& st,
                                          uint32_t nowMs,
                                          bool doorOpenNow,
                                          const char*& outFlag) const {
  using namespace ConfigurationSharedTypes;

  outFlag = "";

  if (!Config::AUTO_ARM_ENABLED) return false;
  if (st.mode != Mode::disarm) return false;

  if ((st.exit_stage == 1 || st.exit_stage == 2) &&
      st.exit_timeout_ms != 0 &&
      reached(nowMs, st.exit_timeout_ms)) {
    st.exit_stage = 0;
    st.exit_timeout_ms = 0;
    outFlag = "auto_arm_timeout_reset";
    return true;
  }

  if (st.exit_stage == 2 && !doorOpenNow) {
    st.exit_stage = 3;
    st.exit_timeout_ms = nowMs + Config::EXIT_AUTO_ARM_WINDOW_MS;
    outFlag = "exit_stage_3";
    return true;
  }

  if (st.exit_stage == 3 && reached(nowMs, st.exit_timeout_ms)) {
    setBaseMode(st, Mode::away);
    outFlag = "mode_away";
    return true;
  }

  return false;
}

} // namespace ApplicationLogicLayer
