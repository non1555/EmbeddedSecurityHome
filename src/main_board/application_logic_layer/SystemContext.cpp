#include "application_logic_layer/SystemContext.h"

#include <cstring>

#include "application_logic_layer/EventCollector.h"
#include "application_logic_layer/RuleEngine.h"
#include "configuration_shared_types/Config.h"

namespace ApplicationLogicLayer {

bool SystemContext::reached_(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

void SystemContext::copyText_(char* out, size_t outLen, const char* in) {
  if (!out || outLen == 0) return;
  if (!in) {
    out[0] = '\0';
    return;
  }
  std::strncpy(out, in, outLen - 1);
  out[outLen - 1] = '\0';
}

String SystemContext::normalize_(String text) {
  text.trim();
  text.toLowerCase();
  return text;
}

ConfigurationSharedTypes::Mode SystemContext::sanitizeBaseMode_(ConfigurationSharedTypes::Mode mode) {
  return (mode == ConfigurationSharedTypes::Mode::away)
    ? ConfigurationSharedTypes::Mode::away
    : ConfigurationSharedTypes::Mode::disarm;
}

void SystemContext::applyActiveModeFromBase_() {
  state_.latest_mode = sanitizeBaseMode_(state_.latest_mode);
  state_.mode = state_.is_night
    ? ConfigurationSharedTypes::Mode::night
    : state_.latest_mode;
}

void SystemContext::persistModeState_() {
  nvs_.saveModeState(state_.latest_mode, state_.is_night);
}

bool SystemContext::begin() {
  commandQueue_ = xQueueCreate(24, sizeof(ConfigurationSharedTypes::RemoteCommandMessage));
  if (!commandQueue_) {
    return false;
  }

  actuators_.begin();
  nvs_.begin();
  mqtt_.begin(commandQueue_);

  ConfigurationSharedTypes::Mode latestMode = ConfigurationSharedTypes::Mode::disarm;
  bool isNight = false;
  if (nvs_.loadModeState(latestMode, isNight)) {
    state_.latest_mode = sanitizeBaseMode_(latestMode);
    state_.is_night = isNight;
  } else {
    state_.latest_mode = ConfigurationSharedTypes::Mode::disarm;
    state_.is_night = false;
  }

  applyActiveModeFromBase_();
  state_.level = ConfigurationSharedTypes::AlarmLevel::off;

  actuators_.lockAll();
  syncSnapshot_();
  enqueueStatus_("boot");
  nextStatusAtMs_ = millis() + ConfigurationSharedTypes::Config::STATUS_PERIOD_MS;
  return true;
}

void SystemContext::bindCollector(EventCollector* collector) {
  collector_ = collector;
  syncSnapshot_();
}

bool SystemContext::pollEvent(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (pollRemoteEvent(nowMs, out)) {
    return true;
  }

  if (!collector_) return false;
  return collector_->poll(nowMs, out);
}

bool SystemContext::pollRemoteEvent(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  return pollRemoteCommand_(nowMs, out);
}

void SystemContext::applyDecision(const ConfigurationSharedTypes::Event& event,
                                  const ConfigurationSharedTypes::Decision& decision) {
  const ConfigurationSharedTypes::Mode previousMode = state_.mode;
  const ConfigurationSharedTypes::Mode previousLatestMode = state_.latest_mode;
  const bool previousIsNight = state_.is_night;
  state_ = decision.next;
  state_.latest_mode = sanitizeBaseMode_(state_.latest_mode);
  applyActiveModeFromBase_();

  if (state_.mode != previousMode ||
      state_.latest_mode != previousLatestMode ||
      state_.is_night != previousIsNight) {
    persistModeState_();
  }

  if (state_.mode == ConfigurationSharedTypes::Mode::away ||
      state_.mode == ConfigurationSharedTypes::Mode::night) {
    actuators_.lockAll();
    clearDoorSession_(true);
  }

  switch (decision.cmd) {
    case ConfigurationSharedTypes::CommandType::buzzer_warn:
      actuators_.warn();
      break;
    case ConfigurationSharedTypes::CommandType::buzzer_alert:
      actuators_.alert();
      break;
    case ConfigurationSharedTypes::CommandType::buzzer_stop:
      actuators_.silence();
      break;
    case ConfigurationSharedTypes::CommandType::none:
    default:
      break;
  }

  if (state_.mode == ConfigurationSharedTypes::Mode::disarm &&
      decision.cmd == ConfigurationSharedTypes::CommandType::none) {
    actuators_.silence();
  }

  if (event.type == ConfigurationSharedTypes::EventType::door_code_unlock) {
    actuators_.unlockDoor();
    bool doorOpen = false;
    if (collector_) {
      doorOpen = collector_->isDoorOpen();
    }
    startDoorSession_(event.ts_ms, doorOpen);
    state_.failed_attempts = 0;
  }

  enqueueEvent_(event, decision.flag);
  enqueueStatus_((decision.flag && decision.flag[0])
                   ? decision.flag
                   : ConfigurationSharedTypes::toString(event.type));
}

void SystemContext::updateActuators(uint32_t nowMs, const RuleEngine& engine) {
  actuators_.update(nowMs);

  syncSnapshot_();
  if (pendingDoorLockStatus_.active && state_.door_locked) {
    enqueueStatus_(pendingDoorLockStatus_.reason);
    pendingDoorLockStatus_ = PendingDoorLockStatus{};
    syncSnapshot_();
  }
  updateDoorSession_(nowMs, state_.door_open);

  const ConfigurationSharedTypes::Mode previousMode = state_.mode;
  const ConfigurationSharedTypes::Mode previousLatestMode = state_.latest_mode;
  const bool previousIsNight = state_.is_night;
  const char* tickFlag = "";
  if (engine.processDisarmAutoArmTick(state_, nowMs, state_.door_open, tickFlag)) {
    state_.latest_mode = sanitizeBaseMode_(state_.latest_mode);
    applyActiveModeFromBase_();
    if (state_.mode != previousMode ||
        state_.latest_mode != previousLatestMode ||
        state_.is_night != previousIsNight) {
      persistModeState_();
    }
    if ((state_.mode == ConfigurationSharedTypes::Mode::away ||
         state_.mode == ConfigurationSharedTypes::Mode::night) &&
        state_.mode != previousMode) {
      actuators_.lockAll();
      clearDoorSession_(true);
    }
    enqueueStatus_(tickFlag);
  }

  syncSnapshot_();

  if (collector_) {
    bool countdownActive = false;
    uint32_t countdownDeadline = 0;
    uint32_t countdownWarnBefore = 0;
    countdownView_(nowMs, countdownActive, countdownDeadline, countdownWarnBefore);
    collector_->updateDisplay(nowMs,
                              state_.door_locked,
                              state_.door_open,
                              countdownActive,
                              countdownDeadline,
                              countdownWarnBefore);
  }

  if (reached_(nowMs, nextStatusAtMs_)) {
    nextStatusAtMs_ = nowMs + ConfigurationSharedTypes::Config::STATUS_PERIOD_MS;
    enqueueStatus_("periodic");
  }
}

void SystemContext::mqttTick(uint32_t nowMs) {
  mqtt_.loop(nowMs);
}

bool SystemContext::isMqttConnected() {
  return mqtt_.isConnected();
}

void SystemContext::handleSilenceRequest() {
  if (doorSession_.active && state_.door_open) {
    doorSession_.holdWarnSilenced = true;
  }
  actuators_.silence();
  enqueueStatus_("silence");
}

void SystemContext::handleHelpRequest(const ConfigurationSharedTypes::Event& event) {
  state_.level = ConfigurationSharedTypes::AlarmLevel::alert;
  actuators_.alert();
  enqueueEvent_(event, "keypad_help");
  enqueueStatus_("keypad_help");
}

void SystemContext::triggerWarn_(const char* reason) {
  state_.level = ConfigurationSharedTypes::AlarmLevel::warn;
  actuators_.warn();
  enqueueStatus_(reason);
}

void SystemContext::handleManualToggle(const ConfigurationSharedTypes::Event& event) {
  syncSnapshot_();

  if (event.type == ConfigurationSharedTypes::EventType::manual_door_toggle) {
    if (state_.door_locked) {
      actuators_.unlockDoor();
      bool doorOpen = collector_ ? collector_->isDoorOpen() : false;
      clearDoorSession_(true);
      startDoorSession_(event.ts_ms, doorOpen);
      enqueueEvent_(event, "manual_door_unlock");
      enqueueStatus_("manual_door_unlock");
      return;
    }

    if (state_.door_open) {
      triggerWarn_("warn_lock_door_open");
      return;
    }

    actuators_.lockDoor();
    clearDoorSession_(true);
    enqueueEvent_(event, "manual_door_lock");
    enqueueStatus_("manual_door_lock");
    return;
  }

  if (event.type == ConfigurationSharedTypes::EventType::manual_window_toggle) {
    if (state_.window_locked) {
      actuators_.unlockWindow();
      enqueueEvent_(event, "manual_window_unlock");
      enqueueStatus_("manual_window_unlock");
      return;
    }

    if (state_.window_open) {
      triggerWarn_("warn_lock_window_open");
      return;
    }

    actuators_.lockWindow();
    enqueueEvent_(event, "manual_window_lock");
    enqueueStatus_("manual_window_lock");
  }
}

ConfigurationSharedTypes::SystemState& SystemContext::state() {
  return state_;
}

const ConfigurationSharedTypes::SystemState& SystemContext::state() const {
  return state_;
}

bool SystemContext::pollRemoteCommand_(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  constexpr uint8_t kMaxCmdDrain = 1;

  uint8_t drained = 0;
  ConfigurationSharedTypes::RemoteCommandMessage msg{};
  while (drained < kMaxCmdDrain && xQueueReceive(commandQueue_, &msg, 0) == pdTRUE) {
    ++drained;
    String command = normalize_(String(msg.payload));
    if (command.length() == 0) continue;

    if (command == "status") {
      enqueueAck_("status", true, "ok");
      enqueueStatus_("remote_status");
      continue;
    }

    if (command == "keypad_help") {
      enqueueAck_("keypad_help", true, "ok");
      out = ConfigurationSharedTypes::Event{
        ConfigurationSharedTypes::EventType::keypad_help_request,
        nowMs,
        9
      };
      return true;
    }

    if (command == "silence" || command == "alarm off" || command == "buzzer stop") {
      actuators_.silence();
      enqueueAck_("silence", true, "ok");
      enqueueStatus_("remote_silence");
      continue;
    }

    if (command == "arm night" || command == "arm_night" || command == "mode night") {
      if (sanitizeBaseMode_(state_.latest_mode) != ConfigurationSharedTypes::Mode::disarm) {
        enqueueAck_("arm_night", false, "allowed_only_when_latest_disarm");
        continue;
      }
      enqueueAck_("arm_night", true, "ok");
      out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::arm_night, nowMs, 9};
      return true;
    }

    if (command == "night_off" || command == "night off") {
      if (state_.mode != ConfigurationSharedTypes::Mode::night) {
        enqueueAck_("night_off", false, "allowed_only_in_night");
        continue;
      }

      enqueueAck_("night_off", true, "ok");
      const ConfigurationSharedTypes::Mode baseMode = sanitizeBaseMode_(state_.latest_mode);
      out = ConfigurationSharedTypes::Event{
        (baseMode == ConfigurationSharedTypes::Mode::away)
          ? ConfigurationSharedTypes::EventType::arm_away
          : ConfigurationSharedTypes::EventType::disarm,
        nowMs,
        9
      };
      return true;
    }

    if (command == "lock door") {
      if (state_.door_open) {
        enqueueAck_("lock door", false, "door_open");
        triggerWarn_("warn_lock_door_open");
        continue;
      }
      actuators_.lockDoor();
      clearDoorSession_(true);
      enqueueAck_("lock door", true, "ok");
      enqueueStatus_("remote_lock_door");
      continue;
    }

    if (command == "unlock door") {
      actuators_.unlockDoor();
      bool doorOpen = collector_ ? collector_->isDoorOpen() : false;
      startDoorSession_(nowMs, doorOpen);
      enqueueAck_("unlock door", true, "ok");
      enqueueStatus_("remote_unlock_door");
      continue;
    }

    if (command == "lock window") {
      if (state_.window_open) {
        enqueueAck_("lock window", false, "window_open");
        triggerWarn_("warn_lock_window_open");
        continue;
      }
      actuators_.lockWindow();
      enqueueAck_("lock window", true, "ok");
      enqueueStatus_("remote_lock_window");
      continue;
    }

    if (command == "unlock window") {
      actuators_.unlockWindow();
      enqueueAck_("unlock window", true, "ok");
      enqueueStatus_("remote_unlock_window");
      continue;
    }

    if (command == "lock all") {
      if (state_.door_open || state_.window_open) {
        enqueueAck_("lock all", false, state_.door_open && state_.window_open
          ? "door_and_window_open"
          : (state_.door_open ? "door_open" : "window_open"));
        triggerWarn_("warn_lock_all_open");
        continue;
      }
      actuators_.lockAll();
      clearDoorSession_(true);
      enqueueAck_("lock all", true, "ok");
      enqueueStatus_("remote_lock_all");
      continue;
    }

    if (command == "unlock all") {
      actuators_.unlockAll();
      bool doorOpen = collector_ ? collector_->isDoorOpen() : false;
      startDoorSession_(nowMs, doorOpen);
      enqueueAck_("unlock all", true, "ok");
      enqueueStatus_("remote_unlock_all");
      continue;
    }
  }

  return false;
}

void SystemContext::syncSnapshot_() {
  if (collector_) {
    state_.door_open = collector_->isDoorOpen();
    state_.window_open = collector_->isWindowOpen();
  }
  state_.door_locked = actuators_.isDoorLocked();
  state_.window_locked = actuators_.isWindowLocked();
}

void SystemContext::startDoorSession_(uint32_t nowMs, bool doorOpen) {
  pendingDoorLockStatus_ = PendingDoorLockStatus{};
  doorSession_.active = true;
  doorSession_.sawOpen = doorOpen;
  doorSession_.lastDoorOpen = doorOpen;
  doorSession_.holdWarnSilenced = false;
  doorSession_.unlockDeadlineMs = nowMs + ConfigurationSharedTypes::Config::DOOR_UNLOCK_TIMEOUT_MS;
  doorSession_.openWarnAtMs = doorOpen
    ? (nowMs + ConfigurationSharedTypes::Config::DOOR_OPEN_HOLD_WARN_AFTER_MS)
    : 0;
  doorSession_.closeLockAtMs = 0;
  doorSession_.nextWarnMs = 0;
}

void SystemContext::clearDoorSession_(bool silenceBuzzer) {
  doorSession_ = DoorSession{};
  if (silenceBuzzer) {
    actuators_.silence();
  }
}

void SystemContext::updateDoorSession_(uint32_t nowMs, bool doorOpen) {
  if (!doorSession_.active) return;

  if (!doorSession_.lastDoorOpen && doorOpen) {
    doorSession_.sawOpen = true;
    doorSession_.holdWarnSilenced = false;
    doorSession_.openWarnAtMs = nowMs + ConfigurationSharedTypes::Config::DOOR_OPEN_HOLD_WARN_AFTER_MS;
    doorSession_.closeLockAtMs = 0;
    doorSession_.nextWarnMs = 0;
  }

  if (doorSession_.lastDoorOpen && !doorOpen) {
    doorSession_.holdWarnSilenced = false;
    doorSession_.openWarnAtMs = 0;
    doorSession_.closeLockAtMs = nowMs + ConfigurationSharedTypes::Config::DOOR_AUTOLOCK_AFTER_CLOSE_MS;
    doorSession_.nextWarnMs = 0;
  }
  doorSession_.lastDoorOpen = doorOpen;

  if (doorSession_.closeLockAtMs != 0) {
    if (doorOpen) {
      doorSession_.closeLockAtMs = 0;
    } else if (reached_(nowMs, doorSession_.closeLockAtMs)) {
      actuators_.lockDoor();
      clearDoorSession_(true);
      copyText_(pendingDoorLockStatus_.reason,
                sizeof(pendingDoorLockStatus_.reason),
                "auto_locked");
      pendingDoorLockStatus_.active = true;
    }
    return;
  }

  if (!doorSession_.sawOpen) {
    if (reached_(nowMs, doorSession_.unlockDeadlineMs)) {
      actuators_.lockDoor();
      clearDoorSession_(true);
      copyText_(pendingDoorLockStatus_.reason,
                sizeof(pendingDoorLockStatus_.reason),
                "auto_locked_timeout");
      pendingDoorLockStatus_.active = true;
      return;
    }

    const uint32_t left = doorSession_.unlockDeadlineMs - nowMs;
    if (left <= ConfigurationSharedTypes::Config::DOOR_UNLOCK_WARN_BEFORE_MS &&
        (doorSession_.nextWarnMs == 0 || reached_(nowMs, doorSession_.nextWarnMs))) {
      actuators_.warn();
      doorSession_.nextWarnMs = nowMs + ConfigurationSharedTypes::Config::DOOR_WARN_RETRIGGER_MS;
    }
    return;
  }

  if (doorOpen &&
      doorSession_.openWarnAtMs != 0 &&
      reached_(nowMs, doorSession_.openWarnAtMs) &&
      !doorSession_.holdWarnSilenced &&
      (doorSession_.nextWarnMs == 0 || reached_(nowMs, doorSession_.nextWarnMs))) {
    actuators_.warn();
    doorSession_.nextWarnMs = nowMs + ConfigurationSharedTypes::Config::DOOR_WARN_RETRIGGER_MS;
  }
}

void SystemContext::countdownView_(uint32_t nowMs,
                                   bool& active,
                                   uint32_t& deadlineMs,
                                   uint32_t& warnBeforeMs) const {
  active = false;
  deadlineMs = 0;
  warnBeforeMs = 0;
  if (!doorSession_.active || state_.door_locked) return;

  if (doorSession_.closeLockAtMs != 0 && (int32_t)(doorSession_.closeLockAtMs - nowMs) > 0) {
    active = true;
    deadlineMs = doorSession_.closeLockAtMs;
    warnBeforeMs = 1000;
    return;
  }

  if (!doorSession_.sawOpen && (int32_t)(doorSession_.unlockDeadlineMs - nowMs) > 0) {
    active = true;
    deadlineMs = doorSession_.unlockDeadlineMs;
    warnBeforeMs = ConfigurationSharedTypes::Config::DOOR_UNLOCK_WARN_BEFORE_MS;
    return;
  }

  if (state_.door_open &&
      doorSession_.openWarnAtMs != 0 &&
      (int32_t)(doorSession_.openWarnAtMs - nowMs) > 0) {
    active = true;
    deadlineMs = doorSession_.openWarnAtMs;
    warnBeforeMs = 2000;
  }
}

void SystemContext::enqueueStatus_(const char* reason) {
  syncSnapshot_();
  Serial.print("[SERIAL] publish status_reason=");
  Serial.println((reason && reason[0]) ? reason : "-");
  ConfigurationSharedTypes::PublishMessage msg{};
  msg.kind = ConfigurationSharedTypes::PublishKind::status;
  msg.st = state_;
  copyText_(msg.text1, sizeof(msg.text1), reason);
  mqtt_.enqueue(msg);
}

void SystemContext::enqueueEvent_(const ConfigurationSharedTypes::Event& event, const char* flag) {
  syncSnapshot_();
  Serial.print("[SERIAL] publish event_flag=");
  Serial.println((flag && flag[0]) ? flag : "-");
  ConfigurationSharedTypes::PublishMessage msg{};
  msg.kind = ConfigurationSharedTypes::PublishKind::event;
  msg.event = event;
  msg.st = state_;
  copyText_(msg.text1, sizeof(msg.text1), flag);
  mqtt_.enqueue(msg);
}

void SystemContext::enqueueAck_(const char* command, bool ok, const char* detail) {
  ConfigurationSharedTypes::PublishMessage msg{};
  msg.kind = ConfigurationSharedTypes::PublishKind::ack;
  msg.ok = ok;
  copyText_(msg.text1, sizeof(msg.text1), command);
  copyText_(msg.text2, sizeof(msg.text2), detail);
  mqtt_.enqueue(msg);
}

} // namespace ApplicationLogicLayer
