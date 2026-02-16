#include "app/SecurityOrchestrator.h"

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
} // namespace

void SecurityOrchestrator::printEventDecision(const Event& e, const Decision& d) const {
  Serial.print("[EV] "); Serial.print(toString(e.type));
  Serial.print(" src="); Serial.print((int)e.src);
  Serial.print(" | [CMD] "); Serial.print(toString(d.cmd.type));
  Serial.print(" | MODE "); Serial.print(toString(d.next.mode));
  Serial.print(" | LEVEL "); Serial.print(toString(d.next.level));
  Serial.print(" | ENTRY "); Serial.println(d.next.entry_pending ? "pending" : "none");
}

void SecurityOrchestrator::applyDecision(const Event& e) {
  const Decision d = engine_.handle(state_, cfg_, e);
  state_ = d.next;
  applyCommand(d.cmd, state_, acts_, &notifySvc_, &logger_);
  if (state_.mode != Mode::disarm) clearDoorUnlockSession(true);
  printEventDecision(e, d);
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

void SecurityOrchestrator::begin() {
  logger_.begin();
  notifySvc_.begin();

  collector_.begin();
  mqttBus_.begin();

  buzzer_.begin();
  servo1_.begin();
  servo2_.begin();
  servo1WasLocked_ = servo1_.isLocked();

  Serial.println("READY");
  Serial.println("Serial keys: 0=DISARM 1=ARM_NIGHT 6=ARM_AWAY 8=DOOR_OPEN 2=WINDOW_OPEN 7=DOOR_TAMPER 3=VIB 4=MOTION 5=CHOKEPOINT S=SILENCE_DOOR_HOLD_WARN D=DOOR_TOGGLE W=WINDOW_TOGGLE");
  Serial.println("Policy: keypad code disarms when armed; unlocks door when disarmed.");
  Serial.printf("Manual toggle button pins (active LOW): DOOR=%u WINDOW=%u\n",
                HwCfg::PIN_BTN_DOOR_TOGGLE,
                HwCfg::PIN_BTN_WINDOW_TOGGLE);
}

void SecurityOrchestrator::processRemoteCommand(const String& payload) {
  const String cmd = normalize(payload);
  const uint32_t nowMs = millis();
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

  // Buzzer/alarm test commands (useful when outputs aren't wired yet)
  if (cmd == "buzz" || cmd == "buzzer" || cmd == "buzz warn" || cmd == "buzzer warn") {
    buzzer_.warn();
    Serial.println("[REMOTE] buzzer warn");
    mqttBus_.publishAck("buzz warn", true, "ok");
    return;
  }

  if (cmd == "alarm" || cmd == "alarm on" || cmd == "buzz alarm" || cmd == "buzz alert" || cmd == "buzzer alert") {
    buzzer_.alert();
    Serial.println("[REMOTE] buzzer alert");
    mqttBus_.publishAck("alarm", true, "ok");
    return;
  }

  if (cmd == "silence" || cmd == "alarm off" || cmd == "buzz stop" || cmd == "buzzer stop") {
    buzzer_.stop();
    Serial.println("[REMOTE] buzzer stop");
    mqttBus_.publishAck("silence", true, "ok");
    return;
  }

  if (cmd == "disarm" || cmd == "mode disarm") {
    const Event e{EventType::disarm, millis(), 9};
    applyDecision(e);
    Serial.printf("[REMOTE] mode command=%s -> mode=%d level=%d\n", cmd.c_str(), (int)state_.mode, (int)state_.level);
    mqttBus_.publishAck("disarm", true, "ok");
    return;
  }

  if (cmd == "arm night" || cmd == "arm_night" || cmd == "mode night") {
    const Event e{EventType::arm_night, millis(), 9};
    applyDecision(e);
    Serial.printf("[REMOTE] mode command=%s -> mode=%d level=%d\n", cmd.c_str(), (int)state_.mode, (int)state_.level);
    mqttBus_.publishAck("arm night", true, "ok");
    return;
  }

  if (cmd == "arm away" || cmd == "arm_away" || cmd == "mode away") {
    const Event e{EventType::arm_away, millis(), 9};
    applyDecision(e);
    Serial.printf("[REMOTE] mode command=%s -> mode=%d level=%d\n", cmd.c_str(), (int)state_.mode, (int)state_.level);
    mqttBus_.publishAck("arm away", true, "ok");
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
    return;
  }

  if (cmd == "lock window") {
    const bool ok = tryLockWindow(collector_, servo2_, notifySvc_, "lock window rejected");
    if (!ok) {
      mqttBus_.publishAck("lock window", false, "window open");
      return;
    }
    state_.keep_window_locked_when_disarmed = true;
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("lock window", true, detail);
    return;
  }

  if (cmd == "lock all") {
    if (collector_.isDoorOpen()) {
      notifySvc_.send("lock all rejected: door is open");
      mqttBus_.publishAck("lock all", false, "door open");
      return;
    }
    if (collector_.isWindowOpen()) {
      notifySvc_.send("lock all rejected: window is open");
      mqttBus_.publishAck("lock all", false, "window open");
      return;
    }
    servo1_.lock();
    clearDoorUnlockSession(true);
    servo2_.lock();
    state_.keep_window_locked_when_disarmed = true;
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("lock all", true, detail);
    return;
  }

  if (cmd == "unlock door") {
    servo1_.unlock();
    clearDoorUnlockSession(true);
    if (!collector_.isDoorOpen()) startDoorUnlockSession(nowMs);
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("unlock door", true, detail);
    return;
  }

  if (cmd == "unlock window") {
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("unlock window", true, detail);
    return;
  }

  if (cmd == "unlock all") {
    servo1_.unlock();
    clearDoorUnlockSession(true);
    if (!collector_.isDoorOpen()) startDoorUnlockSession(nowMs);
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    char detail[32];
    fillDetail(detail, sizeof(detail));
    mqttBus_.publishAck("unlock all", true, detail);
    return;
  }

  mqttBus_.publishAck("unknown", false, "unsupported command");
}

bool SecurityOrchestrator::processDoorHoldWarnSilenceEvent(const Event& e) {
  if (e.type != EventType::door_hold_warn_silence) return false;

  if (!doorSession_.silenceHoldWarning(collector_.isDoorOpen(), buzzer_, notifySvc_)) {
    Serial.println("[KEYPAD] silence ignored (not in door-open-hold warning)");
  }
  return true;
}

bool SecurityOrchestrator::processManualActuatorEvent(const Event& e) {
  if (e.type == EventType::manual_door_toggle) {
    if (servo1_.isLocked()) {
      servo1_.unlock();
      clearDoorUnlockSession(true);
      if (!collector_.isDoorOpen()) startDoorUnlockSession(e.ts_ms);
      notifySvc_.send("manual door: unlocked");
      return true;
    }
    // toggle -> lock
    if (collector_.isDoorOpen()) {
      notifySvc_.send("manual door lock rejected: door is open");
      return true;
    }
    servo1_.lock();
    clearDoorUnlockSession(true);
    notifySvc_.send("manual door: locked");
    return true;
  }

  if (e.type == EventType::manual_window_toggle) {
    if (servo2_.isLocked()) {
      state_.keep_window_locked_when_disarmed = false;
      servo2_.unlock();
      notifySvc_.send("manual window: unlocked");
      return true;
    }
    if (collector_.isWindowOpen()) {
      notifySvc_.send("manual window lock rejected: window is open");
      return true;
    }
    state_.keep_window_locked_when_disarmed = true;
    servo2_.lock();
    notifySvc_.send("manual window: locked");
    return true;
  }

  if (e.type == EventType::manual_lock_request) {
    if (collector_.isDoorOpen()) {
      notifySvc_.send("manual lock rejected: door is open");
      return true;
    }
    if (collector_.isWindowOpen()) {
      notifySvc_.send("manual lock rejected: window is open");
      return true;
    }
    servo2_.lock();
    state_.keep_window_locked_when_disarmed = true;
    servo1_.lock();
    clearDoorUnlockSession(true);
    notifySvc_.send("manual lock accepted");
    return true;
  }

  if (e.type == EventType::manual_unlock_request) {
    servo1_.unlock();
    clearDoorUnlockSession(true);
    if (!collector_.isDoorOpen()) startDoorUnlockSession(e.ts_ms);
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    notifySvc_.send("manual unlock accepted");
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
    if (e.type == EventType::door_code_bad) {
      // 3 strikes: notify each time, last time trigger buzzer alert.
      if (badDoorCodeAttempts_ < 3) badDoorCodeAttempts_++;
      const uint8_t n = badDoorCodeAttempts_;
      const bool last = (n >= 3);

      const String msg = String("wrong door code ") + String(n) + "/3" + (last ? " (ALERT)" : "");
      notifySvc_.send(msg);
      mqttBus_.publishAck("door_code", false, msg.c_str());
      if (last) {
        buzzer_.alert();
        badDoorCodeAttempts_ = 0; // allow next 3 tries
      }
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (e.type == EventType::door_code_unlock) {
      // Always issue a DISARM event first. It's idempotent when already disarmed,
      // and guarantees "code -> disarm" works even if mode tracking gets out of sync.
      const Mode before = state_.mode;
      badDoorCodeAttempts_ = 0;
      applyDecision({EventType::disarm, nowMs, 9});

      // Variant B: disarm + unlock door (entry UX).
      servo1_.unlock();
      // Keep window locked while disarmed; allow a timed door-unlock session.
      state_.keep_window_locked_when_disarmed = true;
      servo2_.lock();
      clearDoorUnlockSession(true);
      if (!collector_.isDoorOpen()) startDoorUnlockSession(nowMs);

      if (before == Mode::disarm) notifySvc_.send("door code accepted");
      else notifySvc_.send("disarmed by code");

      if (state_.mode != Mode::disarm) {
        Serial.print("[KEYPAD] WARN: disarm by code failed, mode=");
        Serial.println(toString(state_.mode));
      }
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (EventGate::allowKeypadEvent(e)) {
      applyDecision(e);
      if (e.type == EventType::disarm) {
        state_.keep_window_locked_when_disarmed = true;
        servo2_.lock();
        if (!collector_.isDoorOpen()) startDoorUnlockSession(nowMs);
      }
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
  if (processDoorHoldWarnSilenceEvent(e)) return;
  if (processManualActuatorEvent(e)) return;
  applyDecision(e);
}
