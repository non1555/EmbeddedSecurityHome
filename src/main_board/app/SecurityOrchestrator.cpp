#include "app/SecurityOrchestrator.h"

#ifndef FW_CMD_TOKEN
#define FW_CMD_TOKEN ""
#endif

namespace {
static String normalize(String s) {
  s.trim();
  s.toLowerCase();
  return s;
}

static bool tryLockDoor(EventCollector& collector, Servo& servo, Notify& notify, const char* reason) {
  if (collector.isDoorOpen()) {
    notify.send(String(reason) + ": door is open");
    return false;
  }
  servo.lock();
  return true;
}

static bool tryLockWindow(EventCollector& collector, Servo& servo, Notify& notify, const char* reason) {
  if (collector.isWindowOpen()) {
    notify.send(String(reason) + ": window is open");
    return false;
  }
  servo.lock();
  return true;
}

static bool isModeEvent(EventType t) {
  return t == EventType::disarm || t == EventType::arm_away;
}

static bool isArmedMode(Mode mode) {
  return mode == Mode::away;
}

static bool unlockAllowed(Mode mode) {
  return mode == Mode::disarm;
}

static bool isManualActuatorEvent(EventType t) {
  return t == EventType::manual_door_toggle ||
         t == EventType::manual_window_toggle;
}

static bool isSerialSyntheticSensorEvent(EventType t) {
  return t == EventType::door_open ||
         t == EventType::window_open ||
         t == EventType::door_tamper ||
         t == EventType::vib_spike ||
         t == EventType::motion ||
         t == EventType::chokepoint;
}

static bool isReadOnlyRemoteCommand(const String& cmd) {
  return cmd == "status";
}

static bool parseAuthorizedRemoteCommand(const String& payload,
                                         const String& configuredToken,
                                         bool requireNonce,
                                         String& outNonce,
                                         String& outCmd) {
  outNonce = "";
  if (configuredToken.length() == 0) {
    if (requireNonce) return false;
    outCmd = normalize(payload);
    return outCmd.length() > 0;
  }

  const int firstSep = payload.indexOf('|');
  if (firstSep <= 0) return false;

  String presentedToken = payload.substring(0, firstSep);
  presentedToken = normalize(presentedToken);
  if (presentedToken != configuredToken) return false;

  const int secondSep = payload.indexOf('|', firstSep + 1);
  if (secondSep < 0) {
    if (requireNonce) return false;
    String commandPart = payload.substring(firstSep + 1);
    commandPart = normalize(commandPart);
    if (commandPart.length() == 0) return false;
    outCmd = commandPart;
    return true;
  }

  String noncePart = payload.substring(firstSep + 1, secondSep);
  String commandPart = payload.substring(secondSep + 1);
  noncePart = normalize(noncePart);
  commandPart = normalize(commandPart);

  if (noncePart.length() == 0 || commandPart.length() == 0) return false;
  outNonce = noncePart;
  outCmd = commandPart;
  return true;
}

static bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

static bool parseUint32Strict(const String& s, uint32_t& out) {
  if (s.length() == 0) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c < '0' || c > '9') return false;
    v = (v * 10u) + (uint64_t)(c - '0');
    if (v > 0xFFFFFFFFull) return false;
  }
  out = (uint32_t)v;
  return true;
}

static uint8_t persistModeValue(Mode mode) {
  switch (mode) {
    case Mode::disarm: return 1;
    case Mode::away:   return 2;
    default:           return 0;
  }
}

static bool restoreModeValue(uint8_t value, Mode& outMode) {
  switch (value) {
    case 1: outMode = Mode::disarm; return true;
    case 2: outMode = Mode::away;   return true;
    default:                        return false;
  }
}

constexpr uint32_t STATUS_HEARTBEAT_MS = 5000;
} // namespace

void SecurityOrchestrator::printEventDecision(const Event& e,
                                              const Decision& d,
                                              const SystemState& prev) const {
  Serial.print("[TRACE] event.type="); Serial.println(toString(e.type));
  if (e.src != 0) {
    Serial.print("[TRACE] event.src="); Serial.println((int)e.src);
  }
  if (d.cmd.type != CommandType::none) {
    Serial.print("[TRACE] command.type="); Serial.println(toString(d.cmd.type));
  }
  if (d.next.mode != prev.mode) {
    Serial.print("[TRACE] state.mode="); Serial.println(toString(d.next.mode));
  }
  if (d.next.level != prev.level) {
    Serial.print("[TRACE] state.level="); Serial.println(toString(d.next.level));
  }
  if (d.next.entry_pending != prev.entry_pending) {
    Serial.print("[TRACE] state.entry_pending="); Serial.println(d.next.entry_pending ? "1" : "0");
  }
  if (d.next.suspicion_score != prev.suspicion_score) {
    Serial.print("[TRACE] state.score="); Serial.println((int)d.next.suspicion_score);
  }
  if (state_.door_locked != prev.door_locked) {
    Serial.print("[TRACE] output.door_locked="); Serial.println(state_.door_locked ? "1" : "0");
  }
  if (state_.window_locked != prev.window_locked) {
    Serial.print("[TRACE] output.window_locked="); Serial.println(state_.window_locked ? "1" : "0");
  }
  if (state_.door_open != prev.door_open) {
    Serial.print("[TRACE] output.door_open="); Serial.println(state_.door_open ? "1" : "0");
  }
  if (state_.window_open != prev.window_open) {
    Serial.print("[TRACE] output.window_open="); Serial.println(state_.window_open ? "1" : "0");
  }
}

void SecurityOrchestrator::syncLiveSnapshot() {
  state_.door_locked = servo1_.isLocked();
  state_.window_locked = servo2_.isLocked();
  state_.door_open = collector_.isDoorOpen();
  state_.window_open = collector_.isWindowOpen();
}

void SecurityOrchestrator::publishStateStatus(const char* reason) {
  syncLiveSnapshot();
  mqttBus_.publishStatus(state_, reason);
}

void SecurityOrchestrator::publishStateEvent(const Event& e, const Command& cmd) {
  syncLiveSnapshot();
  mqttBus_.publishEvent(e, state_, cmd);
}

void SecurityOrchestrator::applyDecision(const Event& e) {
  // Use live actuator/sensor state for decision conditions (e.g. forced-open while locked).
  syncLiveSnapshot();
  const SystemState prevState = state_;
  const Mode prevMode = state_.mode;
  const Decision d = engine_.handle(state_, cfg_, e);
  state_ = d.next;
  persistModeIfChanged(prevMode);
  applyCommand(d.cmd, state_, acts_, &notifySvc_, &logger_);
  if (isArmedMode(state_.mode)) clearDoorUnlockSession(true);
  publishStateEvent(e, d.cmd);
  publishStateStatus(toString(e.type));
  printEventDecision(e, d, prevState);
}

void SecurityOrchestrator::startDoorUnlockSession(uint32_t nowMs) {
  doorSession_.start(nowMs, collector_.isDoorOpen(), cfg_);
}

void SecurityOrchestrator::clearDoorUnlockSession(bool stopBuzzer) {
  doorSession_.clear(stopBuzzer, buzzer_);
}

void SecurityOrchestrator::updateDoorUnlockSession(uint32_t nowMs) {
  doorSession_.update(nowMs,
                      collector_.isDoorOpen(),
                      cfg_,
                      servo1_,
                      buzzer_,
                      notifySvc_);
}

void SecurityOrchestrator::restorePersistedMode() {
  if (!noncePrefReady_ || !noncePref_.isKey("mode")) return;

  const uint8_t saved = noncePref_.getUChar("mode", 0);
  Mode restored = Mode::disarm;
  if (!restoreModeValue(saved, restored)) {
    notifySvc_.send("WARN: persisted mode invalid; fallback to disarm");
    return;
  }

  state_.mode = restored;
  state_.level = AlarmLevel::off;
  state_.entry_pending = false;
  state_.entry_deadline_ms = 0;
  state_.suspicion_score = 0;
  state_.last_suspicion_update_ms = millis();
  state_.last_outdoor_motion_ms = 0;
  state_.last_window_event_ms = 0;
  state_.last_vibration_ms = 0;
  state_.last_door_event_ms = 0;
  state_.keep_window_locked_when_disarmed = false;

  Serial.print("[BOOT] restored mode=");
  Serial.println(toString(state_.mode));
}

void SecurityOrchestrator::persistModeIfChanged(Mode prevMode) {
  if (!noncePrefReady_) return;
  if (state_.mode == prevMode) return;

  const uint8_t saved = persistModeValue(state_.mode);
  if (saved == 0) return;
  noncePref_.putUChar("mode", saved);
}

bool SecurityOrchestrator::acceptRemoteNonce(const String& nonce, uint32_t nowMs, bool persistMonotonicFloor) {
  if (!cfg_.require_remote_nonce) return true;

  uint32_t parsed = 0;
  if (cfg_.require_remote_monotonic_nonce) {
    if (!parseUint32Strict(nonce, parsed)) return false;
    if (parsed <= lastRemoteNonce_) return false;
  }

  if (!remoteNonceGuard_.accept(nonce, nowMs, cfg_.remote_nonce_ttl_ms)) {
    return false;
  }

  if (!cfg_.require_remote_monotonic_nonce) return true;

  lastRemoteNonce_ = parsed;
  if (persistMonotonicFloor && noncePrefReady_) {
    noncePref_.putULong("rnonce", lastRemoteNonce_);
  }
  return true;
}

void SecurityOrchestrator::updateSensorHealth(uint32_t nowMs) {
  if (!cfg_.sensor_health_enabled) {
    if (sensorFaultActive_) {
      sensorFaultActive_ = false;
      sensorFaultDetail_ = "";
      notifySvc_.send("sensor health recovered");
      publishStateStatus("sensor_health_recovered");
    }
    return;
  }

  if (nextSensorHealthCheckMs_ != 0 && !reached(nowMs, nextSensorHealthCheckMs_)) return;
  nextSensorHealthCheckMs_ = nowMs + cfg_.sensor_health_check_period_ms;

  EventCollector::HealthSnapshot hs{};
  collector_.readHealth(nowMs,
                        cfg_.pir_stuck_active_ms,
                        cfg_.vib_stuck_active_ms,
                        cfg_.ultrasonic_offline_ms,
                        cfg_.ultrasonic_no_echo_threshold,
                        hs);

  String detail;
  if (hs.pir1_stuck_active) detail += "pir1_stuck;";
  if (hs.pir2_stuck_active) detail += "pir2_stuck;";
  if (hs.pir3_stuck_active) detail += "pir3_stuck;";
  if (hs.vib_stuck_active) detail += "vib_stuck;";
  if (hs.us1_offline) detail += "us1_offline;";
  if (hs.us2_offline) detail += "us2_offline;";
  if (hs.us3_offline) detail += "us3_offline;";

  const bool hasFault = detail.length() > 0;
  if (!hasFault) {
    if (sensorFaultActive_) {
      sensorFaultActive_ = false;
      sensorFaultDetail_ = "";
      notifySvc_.send("sensor health recovered");
      publishStateStatus("sensor_health_recovered");
    }
    return;
  }

  sensorFaultDetail_ = detail;
  const bool shouldNotify = !sensorFaultActive_ ||
                            cfg_.sensor_fault_notify_cooldown_ms == 0 ||
                            reached(nowMs, lastSensorFaultNotifyMs_ + cfg_.sensor_fault_notify_cooldown_ms);
  if (shouldNotify) {
    lastSensorFaultNotifyMs_ = nowMs;
    notifySvc_.send(String("sensor health degraded: ") + sensorFaultDetail_);
    publishStateStatus("sensor_health_fault");
    if (isArmedMode(state_.mode)) {
      buzzer_.warn();
    }
  }
  sensorFaultActive_ = true;
}

void SecurityOrchestrator::begin() {
  logger_.begin();
  notifySvc_.begin();
  notifySvc_.setSerialEnabled(cfg_.serial_notify_enabled);

  collector_.begin();
  mqttBus_.begin();

  noncePrefReady_ = noncePref_.begin("eshsecv2", false);
  if (noncePrefReady_) {
    lastRemoteNonce_ = noncePref_.getULong("rnonce", 0);
    restorePersistedMode();
  } else {
    lastRemoteNonce_ = 0;
    if (cfg_.fail_closed_if_nonce_persistence_unavailable) {
      notifySvc_.send("WARN: nonce persistence disabled; remote mutating commands blocked");
    } else {
      notifySvc_.send("WARN: nonce persistence disabled");
    }
  }

  buzzer_.begin();
  servo1_.begin();
  servo2_.begin();
  if (collector_.isDoorOpen()) {
    notifySvc_.send("startup: door open, skip pre-lock");
  } else {
    servo1_.lock();
  }
  if (collector_.isWindowOpen()) {
    notifySvc_.send("startup: window open, skip pre-lock");
  } else {
    servo2_.lock();
  }
  servo1WasLocked_ = servo1_.isLocked();
  updateSensorHealth(millis());
  publishStateStatus("boot");
  nextStatusHeartbeatMs_ = 0;

  Serial.println("READY");
  Serial.println("Serial test input available. Send '?' for serial code list.");
  collector_.printSerialHelp();
  Serial.println("Policy: keypad code disarms+unlocks.");
  Serial.printf("Manual toggle button pins (active LOW): DOOR=%u WINDOW=%u\n",
                HwCfg::PIN_BTN_DOOR_TOGGLE,
                HwCfg::PIN_BTN_WINDOW_TOGGLE);
}

bool SecurityOrchestrator::processModeEvent(const Event& e, const char* origin) {
  if (!isModeEvent(e.type)) return false;

  applyDecision(e);
  Serial.print("[");
  Serial.print(origin ? origin : "MODE");
  Serial.print("] mode accepted: ");
  Serial.println((int)e.type);
  return true;
}

void SecurityOrchestrator::processRemoteCommand(const String& payload) {
  String cmd;
  String nonce;
  const uint32_t nowMs = millis();
  const String configuredToken = normalize(String(FW_CMD_TOKEN));
  const bool requireNonce = (configuredToken.length() > 0) && cfg_.require_remote_nonce;
  auto fillDetail = [&](char* out, size_t outLen) {
    snprintf(
      out,
      outLen,
      "dL=%u,wL=%u,dO=%u,wO=%u",
      servo1_.isLocked() ? 1u : 0u,
      servo2_.isLocked() ? 1u : 0u,
      collector_.isDoorOpen() ? 1u : 0u,
      collector_.isWindowOpen() ? 1u : 0u
    );
  };
  auto publishRemoteStatus = [&](const char* reason) {
    publishStateStatus(reason);
  };

  if (configuredToken.length() == 0 && !cfg_.allow_remote_without_token) {
    cmd = normalize(payload);
    if (cmd != "status") {
      mqttBus_.publishAck("auth", false, "token required");
      publishRemoteStatus("remote_auth_reject");
      return;
    }
  } else if (!parseAuthorizedRemoteCommand(payload, configuredToken, requireNonce, nonce, cmd)) {
    mqttBus_.publishAck("auth", false, "unauthorized");
    publishRemoteStatus("remote_auth_reject");
    return;
  }

  const bool readOnlyCommand = isReadOnlyRemoteCommand(cmd);
  if (requireNonce &&
      !readOnlyCommand &&
      cfg_.require_remote_monotonic_nonce &&
      cfg_.fail_closed_if_nonce_persistence_unavailable &&
      !noncePrefReady_) {
    mqttBus_.publishAck("auth", false, "nonce storage unavailable");
    publishRemoteStatus("remote_auth_reject_nonce_storage");
    return;
  }

  if (requireNonce && !acceptRemoteNonce(nonce, nowMs, !readOnlyCommand)) {
    mqttBus_.publishAck("auth", false, "replay rejected");
    publishRemoteStatus("remote_replay_reject");
    return;
  }

  // Buzzer/alarm test commands (useful when outputs aren't wired yet)
  if (cmd == "buzz" || cmd == "buzzer" || cmd == "buzz warn" || cmd == "buzzer warn") {
    buzzer_.warn();
    Serial.println("[REMOTE] buzzer warn");
    mqttBus_.publishAck("buzz warn", true, "ok");
    publishRemoteStatus("remote_buzz_warn");
    return;
  }

  if (cmd == "alarm" || cmd == "alarm on" || cmd == "buzz alarm" || cmd == "buzz alert" || cmd == "buzzer alert") {
    buzzer_.alert();
    Serial.println("[REMOTE] buzzer alert");
    mqttBus_.publishAck("alarm", true, "ok");
    publishRemoteStatus("remote_alarm");
    return;
  }

  if (cmd == "silence" || cmd == "alarm off" || cmd == "buzz stop" || cmd == "buzzer stop") {
    buzzer_.stop();
    Serial.println("[REMOTE] buzzer stop");
    mqttBus_.publishAck("silence", true, "ok");
    publishRemoteStatus("remote_silence");
    return;
  }

  if (cmd == "disarm" || cmd == "mode disarm" ||
      cmd == "arm away" || cmd == "arm_away" || cmd == "mode away") {
    EventType t = EventType::disarm;
    const char* ackCmd = "disarm";
    if (cmd == "arm away" || cmd == "arm_away" || cmd == "mode away") {
      t = EventType::arm_away;
      ackCmd = "arm away";
    }
    processModeEvent({t, nowMs, 9}, "REMOTE");
    mqttBus_.publishAck(ackCmd, true, "ok");
    return;
  }

  if (cmd == "status") {
    String msg = "mode=" + String((int)state_.mode) +
                 " level=" + String((int)state_.level) +
                 " door_open=" + String(collector_.isDoorOpen() ? "1" : "0") +
                 " window_open=" + String(collector_.isWindowOpen() ? "1" : "0") +
                 " door_locked=" + String(servo1_.isLocked() ? "1" : "0");
    msg += " window_locked=" + String(servo2_.isLocked() ? "1" : "0");
    notifySvc_.send(msg);
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("status", true, detail);
    publishRemoteStatus("remote_status");
    return;
  }

  if (cmd == "lock door") {
    const bool ok = tryLockDoor(collector_, servo1_, notifySvc_, "lock door rejected");
    if (ok) clearDoorUnlockSession(true);
    if (ok) {
      char detail[32];
      fillDetail(detail, sizeof(detail));
      mqttBus_.publishAck("lock door", true, detail);
    } else {
      mqttBus_.publishAck("lock door", false, "door open");
    }
    publishRemoteStatus(ok ? "remote_lock_door" : "remote_lock_door_reject");
    return;
  }

  if (cmd == "lock window") {
    const bool ok = tryLockWindow(collector_, servo2_, notifySvc_, "lock window rejected");
    if (!ok) {
      mqttBus_.publishAck("lock window", false, "window open");
      publishRemoteStatus("remote_lock_window_reject");
      return;
    }
    state_.keep_window_locked_when_disarmed = true;
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("lock window", true, detail);
    publishRemoteStatus("remote_lock_window");
    return;
  }

  if (cmd == "lock all") {
    if (collector_.isDoorOpen()) {
      notifySvc_.send("lock all rejected: door is open");
      mqttBus_.publishAck("lock all", false, "door open");
      publishRemoteStatus("remote_lock_all_reject_door");
      return;
    }
    if (collector_.isWindowOpen()) {
      notifySvc_.send("lock all rejected: window is open");
      mqttBus_.publishAck("lock all", false, "window open");
      publishRemoteStatus("remote_lock_all_reject_window");
      return;
    }
    servo1_.lock();
    clearDoorUnlockSession(true);
    servo2_.lock();
    state_.keep_window_locked_when_disarmed = true;
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("lock all", true, detail);
    publishRemoteStatus("remote_lock_all");
    return;
  }

  if (cmd == "unlock door") {
    if (cfg_.fail_closed_on_sensor_fault && sensorFaultActive_) {
      notifySvc_.send("unlock door rejected: sensor fault");
      mqttBus_.publishAck("unlock door", false, "sensor fault");
      publishRemoteStatus("remote_unlock_door_reject_sensor_fault");
      return;
    }
    if (!unlockAllowed(state_.mode)) {
      notifySvc_.send("unlock door rejected: disarm required");
      mqttBus_.publishAck("unlock door", false, "disarm required");
      publishRemoteStatus("remote_unlock_door_reject_mode");
      return;
    }
    servo1_.unlock();
    clearDoorUnlockSession(true);
    startDoorUnlockSession(nowMs);
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("unlock door", true, detail);
    publishRemoteStatus("remote_unlock_door");
    return;
  }

  if (cmd == "unlock window") {
    if (cfg_.fail_closed_on_sensor_fault && sensorFaultActive_) {
      notifySvc_.send("unlock window rejected: sensor fault");
      mqttBus_.publishAck("unlock window", false, "sensor fault");
      publishRemoteStatus("remote_unlock_window_reject_sensor_fault");
      return;
    }
    if (!unlockAllowed(state_.mode)) {
      notifySvc_.send("unlock window rejected: disarm required");
      mqttBus_.publishAck("unlock window", false, "disarm required");
      publishRemoteStatus("remote_unlock_window_reject_mode");
      return;
    }
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("unlock window", true, detail);
    publishRemoteStatus("remote_unlock_window");
    return;
  }

  if (cmd == "unlock all") {
    if (cfg_.fail_closed_on_sensor_fault && sensorFaultActive_) {
      notifySvc_.send("unlock all rejected: sensor fault");
      mqttBus_.publishAck("unlock all", false, "sensor fault");
      publishRemoteStatus("remote_unlock_all_reject_sensor_fault");
      return;
    }
    if (!unlockAllowed(state_.mode)) {
      notifySvc_.send("unlock all rejected: disarm required");
      mqttBus_.publishAck("unlock all", false, "disarm required");
      publishRemoteStatus("remote_unlock_all_reject_mode");
      return;
    }
    servo1_.unlock();
    clearDoorUnlockSession(true);
    startDoorUnlockSession(nowMs);
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("unlock all", true, detail);
    publishRemoteStatus("remote_unlock_all");
    return;
  }

  mqttBus_.publishAck("unknown", false, "unsupported command");
  publishRemoteStatus("remote_unknown");
}

bool SecurityOrchestrator::processDoorHoldWarnSilenceEvent(const Event& e) {
  if (e.type != EventType::door_hold_warn_silence) return false;

  if (!doorSession_.silenceHoldWarning(collector_.isDoorOpen(), buzzer_, notifySvc_)) {
    Serial.println("[KEYPAD] silence ignored (not in door-open-hold warning)");
  }
  return true;
}

bool SecurityOrchestrator::processKeypadHelpRequestEvent(const Event& e) {
  if (e.type != EventType::keypad_help_request) return false;

  notifySvc_.send("HELP requested from keypad");
  publishStateEvent(e, {CommandType::none, e.ts_ms});
  publishStateStatus("keypad_help_request");
  return true;
}

bool SecurityOrchestrator::processManualActuatorEvent(const Event& e) {
  auto emitManualTelemetry = [&](const char* reason) {
    publishStateEvent(e, {CommandType::none, e.ts_ms});
    publishStateStatus(reason);
  };

  if (e.type == EventType::manual_door_toggle) {
    if (servo1_.isLocked()) {
      if (cfg_.fail_closed_on_sensor_fault && sensorFaultActive_) {
        notifySvc_.send("manual door unlock blocked: sensor fault");
        emitManualTelemetry("manual_door_unlock_reject_sensor_fault");
        return true;
      }
      if (!unlockAllowed(state_.mode)) {
        notifySvc_.send("manual door unlock blocked: disarm required");
        emitManualTelemetry("manual_door_unlock_reject_mode");
        return true;
      }
      servo1_.unlock();
      clearDoorUnlockSession(true);
      startDoorUnlockSession(e.ts_ms);
      notifySvc_.send("manual door: unlocked");
      emitManualTelemetry("manual_door_unlock");
      return true;
    }
    // toggle -> lock
    if (collector_.isDoorOpen()) {
      notifySvc_.send("manual door lock rejected: door is open");
      emitManualTelemetry("manual_door_lock_reject_open");
      return true;
    }
    servo1_.lock();
    clearDoorUnlockSession(true);
    notifySvc_.send("manual door: locked");
    emitManualTelemetry("manual_door_lock");
    return true;
  }

  if (e.type == EventType::manual_window_toggle) {
    if (servo2_.isLocked()) {
      if (cfg_.fail_closed_on_sensor_fault && sensorFaultActive_) {
        notifySvc_.send("manual window unlock blocked: sensor fault");
        emitManualTelemetry("manual_window_unlock_reject_sensor_fault");
        return true;
      }
      if (!unlockAllowed(state_.mode)) {
        notifySvc_.send("manual window unlock blocked: disarm required");
        emitManualTelemetry("manual_window_unlock_reject_mode");
        return true;
      }
      state_.keep_window_locked_when_disarmed = false;
      servo2_.unlock();
      notifySvc_.send("manual window: unlocked");
      emitManualTelemetry("manual_window_unlock");
      return true;
    }
    if (collector_.isWindowOpen()) {
      notifySvc_.send("manual window lock rejected: window is open");
      emitManualTelemetry("manual_window_lock_reject_open");
      return true;
    }
    state_.keep_window_locked_when_disarmed = true;
    servo2_.lock();
    notifySvc_.send("manual window: locked");
    emitManualTelemetry("manual_window_lock");
    return true;
  }

  return false;
}

void SecurityOrchestrator::tick(uint32_t nowMs) {
  Event e;

  // Always advance actuator patterns even if we return early (keypad/timeout).
  buzzer_.update(nowMs);
  servo1_.update(nowMs);
  servo2_.update(nowMs);

  // If something unlocked the door while it's closed (e.g., DISARM command path),
  // start the auto-lock countdown.
  const bool servo1LockedNow = servo1_.isLocked();
  if (!doorSession_.isActive() &&
      servo1WasLocked_ &&
      !servo1LockedNow &&
      !collector_.isDoorOpen()) {
    startDoorUnlockSession(nowMs);
  }
  servo1WasLocked_ = servo1LockedNow;

  updateSensorHealth(nowMs);
  updateDoorUnlockSession(nowMs);
  if (keypadLockoutUntilMs_ != 0 && reached(nowMs, keypadLockoutUntilMs_)) {
    keypadLockoutUntilMs_ = 0;
    lastKeypadLockoutNotifyMs_ = nowMs;
    notifySvc_.send("keypad lockout expired");
    publishStateStatus("keypad_lockout_expired");
  }

  bool cdActive = false;
  uint32_t cdDeadline = 0;
  uint32_t cdWarnBefore = 0;
  cdActive = doorSession_.countdown(nowMs,
                                    servo1_.isLocked(),
                                    collector_.isDoorOpen(),
                                    cfg_,
                                    cdDeadline,
                                    cdWarnBefore);
  collector_.updateOledStatus(nowMs,
                              servo1_.isLocked(),
                              collector_.isDoorOpen(),
                              cdActive,
                              cdDeadline,
                              cdWarnBefore);

  mqttBus_.update(nowMs);
  if (nextStatusHeartbeatMs_ == 0 || reached(nowMs, nextStatusHeartbeatMs_)) {
    nextStatusHeartbeatMs_ = nowMs + STATUS_HEARTBEAT_MS;
    publishStateStatus("periodic");
  }

  String remoteCmd;
  if (mqttBus_.pollCommand(remoteCmd)) {
    processRemoteCommand(remoteCmd);
    const uint32_t t = millis();
    cdActive = doorSession_.countdown(t,
                                      servo1_.isLocked(),
                                      collector_.isDoorOpen(),
                                      cfg_,
                                      cdDeadline,
                                      cdWarnBefore);
    collector_.updateOledStatus(millis(),
                                servo1_.isLocked(),
                                collector_.isDoorOpen(),
                                cdActive,
                                cdDeadline,
                                cdWarnBefore);
  }

  if (collector_.pollKeypad(nowMs, e)) {
    if (processDoorHoldWarnSilenceEvent(e)) {
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (processKeypadHelpRequestEvent(e)) {
      updateDoorUnlockSession(nowMs);
      return;
    }
    const bool keypadLockedOut =
      (keypadLockoutUntilMs_ != 0) && !reached(nowMs, keypadLockoutUntilMs_);
    const uint8_t badLimit = (cfg_.keypad_bad_attempt_limit == 0) ? 1 : cfg_.keypad_bad_attempt_limit;
    const bool lockoutNotifyDue =
      (lastKeypadLockoutNotifyMs_ == 0) ||
      reached(nowMs, lastKeypadLockoutNotifyMs_ + cfg_.notify_cooldown_ms);
    if (e.type == EventType::door_code_bad) {
      if (keypadLockedOut) {
        if (lockoutNotifyDue) {
          lastKeypadLockoutNotifyMs_ = nowMs;
          notifySvc_.send("door code rejected: keypad lockout active");
        }
        mqttBus_.publishAck("door_code", false, "keypad lockout");
        publishStateStatus("keypad_unlock_reject_lockout");
        updateDoorUnlockSession(nowMs);
        return;
      }

      if (badDoorCodeAttempts_ < badLimit) badDoorCodeAttempts_++;
      const uint8_t n = badDoorCodeAttempts_;
      const bool reachedLimit = (n >= badLimit);

      String msg = String("wrong door code ") + String(n) + "/" + String(badLimit);
      if (reachedLimit) msg += " (ALERT)";
      notifySvc_.send(msg);
      mqttBus_.publishAck("door_code", false, msg.c_str());
      if (reachedLimit) {
        buzzer_.alert();
        if (cfg_.keypad_lockout_ms > 0) {
          keypadLockoutUntilMs_ = nowMs + cfg_.keypad_lockout_ms;
          lastKeypadLockoutNotifyMs_ = nowMs;
          notifySvc_.send("keypad lockout enabled");
          publishStateStatus("keypad_lockout_enabled");
        }
        badDoorCodeAttempts_ = 0;
      }
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (e.type == EventType::door_code_unlock) {
      if (keypadLockedOut) {
        if (lockoutNotifyDue) {
          lastKeypadLockoutNotifyMs_ = nowMs;
          notifySvc_.send("door code accepted: unlock blocked (keypad lockout)");
        }
        mqttBus_.publishAck("door_code", false, "keypad lockout");
        publishStateStatus("keypad_unlock_reject_lockout");
        updateDoorUnlockSession(nowMs);
        return;
      }
      badDoorCodeAttempts_ = 0;
      const bool sensorFaultUnlockOverride =
        cfg_.fail_closed_on_sensor_fault && sensorFaultActive_;
      if (state_.mode != Mode::disarm) {
        processModeEvent({EventType::disarm, nowMs, e.src}, "KEYPAD");
      }
      servo1_.unlock();
      state_.keep_window_locked_when_disarmed = true;
      servo2_.lock();
      clearDoorUnlockSession(true);
      startDoorUnlockSession(nowMs);
      if (sensorFaultUnlockOverride) {
        notifySvc_.send("door code accepted: local unlock override (sensor fault active)");
      } else {
        notifySvc_.send("door code accepted");
      }
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (isModeEvent(e.type)) {
      processModeEvent(e, "KEYPAD");
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (EventGate::allowKeypadEvent(e)) {
      applyDecision(e);
      updateDoorUnlockSession(nowMs);
      return;
    }
    Serial.print("[KEYPAD] command blocked: ");
    Serial.println((int)e.type);
  }

  if (timeoutScheduler_.pollEntryTimeout(state_, nowMs, e)) {
    applyDecision(e);
    return;
  }

  const bool hasEvent = collector_.pollSensorOrSerial(nowMs, e);
  updateDoorUnlockSession(nowMs);
  if (!hasEvent) return;

  if (isSerialSyntheticSource(e.src) && isModeEvent(e.type) && !cfg_.allow_serial_mode_commands) {
    Serial.println("[SERIAL] mode blocked by policy");
    publishStateStatus("serial_mode_blocked");
    return;
  }
  if (isSerialSyntheticSource(e.src) && isManualActuatorEvent(e.type) && !cfg_.allow_serial_manual_commands) {
    Serial.println("[SERIAL] manual actuator blocked by policy");
    publishStateStatus("serial_manual_blocked");
    return;
  }
  if (isSerialSyntheticSource(e.src) && isSerialSyntheticSensorEvent(e.type) && !cfg_.allow_serial_sensor_commands) {
    Serial.println("[SERIAL] sensor event blocked by policy");
    publishStateStatus("serial_sensor_blocked");
    return;
  }

  if (processDoorHoldWarnSilenceEvent(e)) return;
  if (processKeypadHelpRequestEvent(e)) return;
  if (processManualActuatorEvent(e)) return;
  if (isModeEvent(e.type)) {
    processModeEvent(e, "SERIAL");
    return;
  }
  applyDecision(e);
}
