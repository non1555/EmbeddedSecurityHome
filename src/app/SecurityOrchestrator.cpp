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

#if SERVO_COUNT >= 2
static bool tryLockWindow(EventCollector& collector, Servo& servo, Notify& notify, const char* reason) {
  if (collector.isWindowOpen()) {
    notify.send(String(reason) + ": window is open");
    return false;
  }
  servo.lock();
  return true;
}
#endif
} // namespace

void SecurityOrchestrator::printEventDecision(const Event& e, const Decision& d) const {
  Serial.print("[EV] "); Serial.print((int)e.type);
  Serial.print(" src="); Serial.print((int)e.src);
  Serial.print(" | [CMD] "); Serial.print((int)d.cmd.type);
  Serial.print(" | MODE "); Serial.print((int)d.next.mode);
  Serial.print(" | LEVEL "); Serial.print((int)d.next.level);
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
  doorUnlockSessionActive_ = true;
  doorSessionSawOpen_ = false;
  doorHoldWarnActive_ = false;
  doorHoldWarnSilenced_ = false;
  doorUnlockDeadlineMs_ = nowMs + cfg_.door_unlock_timeout_ms;
  doorWasOpenLastTick_ = collector_.isDoorOpen();
  doorOpenSinceMs_ = doorWasOpenLastTick_ ? nowMs : 0;
  nextDoorWarnMs_ = 0;
}

void SecurityOrchestrator::clearDoorUnlockSession(bool stopBuzzer) {
  doorUnlockSessionActive_ = false;
  doorSessionSawOpen_ = false;
  doorHoldWarnActive_ = false;
  doorHoldWarnSilenced_ = false;
  doorUnlockDeadlineMs_ = 0;
  doorOpenSinceMs_ = 0;
  nextDoorWarnMs_ = 0;
  if (stopBuzzer) buzzer_.stop();
}

void SecurityOrchestrator::updateDoorUnlockSession(uint32_t nowMs) {
  if (!doorUnlockSessionActive_) return;

  const bool doorOpen = collector_.isDoorOpen();
  if (!doorWasOpenLastTick_ && doorOpen) {
    doorSessionSawOpen_ = true;
    doorOpenSinceMs_ = nowMs;
    doorHoldWarnActive_ = false;
    doorHoldWarnSilenced_ = false;
    nextDoorWarnMs_ = 0;
  }

  if (doorWasOpenLastTick_ && !doorOpen) {
    servo1_.lock();
    notifySvc_.send("door auto-locked after close");
    clearDoorUnlockSession(true);
    doorWasOpenLastTick_ = doorOpen;
    return;
  }
  doorWasOpenLastTick_ = doorOpen;

  if (doorOpen) {
    if (doorOpenSinceMs_ == 0) doorOpenSinceMs_ = nowMs;
    if ((nowMs - doorOpenSinceMs_) >= cfg_.door_open_hold_warn_after_ms) {
      doorHoldWarnActive_ = true;
      if (!doorHoldWarnSilenced_ && (nextDoorWarnMs_ == 0 || nowMs >= nextDoorWarnMs_)) {
        buzzer_.warn();
        nextDoorWarnMs_ = nowMs + cfg_.door_warn_retrigger_ms;
      }
    }
    return;
  }

  doorOpenSinceMs_ = 0;
  doorHoldWarnActive_ = false;
  doorHoldWarnSilenced_ = false;

  if (nowMs >= doorUnlockDeadlineMs_) {
    servo1_.lock();
    notifySvc_.send("door auto-locked: unlock timeout");
    clearDoorUnlockSession(true);
    return;
  }

  const uint32_t timeLeftMs = doorUnlockDeadlineMs_ - nowMs;
  if (timeLeftMs <= cfg_.door_unlock_warn_before_ms &&
      (nextDoorWarnMs_ == 0 || nowMs >= nextDoorWarnMs_)) {
    buzzer_.warn();
    nextDoorWarnMs_ = nowMs + cfg_.door_warn_retrigger_ms;
  }
}

void SecurityOrchestrator::begin() {
  logger_.begin();
  notifySvc_.begin();

  collector_.begin();
  mqttBus_.begin();

  buzzer_.begin();
  servo1_.begin();
#if SERVO_COUNT >= 2
  servo2_.begin();
#endif

  Serial.println("READY");
  Serial.println("Serial keys: 0=DISARM 1=ARM_NIGHT 6=ARM_AWAY 8=DOOR_OPEN 2=WINDOW_OPEN 7=DOOR_TAMPER 3=VIB 4=MOTION 5=CHOKEPOINT S=SILENCE_DOOR_HOLD_WARN L=MANUAL_LOCK U=MANUAL_UNLOCK");
  Serial.println("Policy: keypad can DISARM only, arm requests are blocked.");
  Serial.printf("Manual button lock/unlock pins: GPIO %u / %u (active LOW)\n",
                HwCfg::PIN_BTN_MANUAL_LOCK,
                HwCfg::PIN_BTN_MANUAL_UNLOCK);
}

void SecurityOrchestrator::processRemoteCommand(const String& payload) {
  const String cmd = normalize(payload);

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
#if SERVO_COUNT >= 2
    msg += " window_locked=" + String(servo2_.isLocked() ? "1" : "0");
#endif
    notifySvc_.send(msg);
    mqttBus_.publishAck("status", true, "ok");
    return;
  }

  if (cmd == "lock door") {
    const bool ok = tryLockDoor(collector_, servo1_, notifySvc_, "lock door rejected");
    if (ok) clearDoorUnlockSession(true);
    mqttBus_.publishAck("lock door", ok, ok ? "ok" : "door open");
    return;
  }

#if SERVO_COUNT >= 2
  if (cmd == "lock window") {
    const bool ok = tryLockWindow(collector_, servo2_, notifySvc_, "lock window rejected");
    if (!ok) {
      mqttBus_.publishAck("lock window", false, "window open");
      return;
    }
    state_.keep_window_locked_when_disarmed = true;
    mqttBus_.publishAck("lock window", true, "ok");
    return;
  }
#endif

  if (cmd == "lock all") {
    if (collector_.isDoorOpen()) {
      notifySvc_.send("lock all rejected: door is open");
      mqttBus_.publishAck("lock all", false, "door open");
      return;
    }
#if SERVO_COUNT >= 2
    if (collector_.isWindowOpen()) {
      notifySvc_.send("lock all rejected: window is open");
      mqttBus_.publishAck("lock all", false, "window open");
      return;
    }
#endif
    servo1_.lock();
    clearDoorUnlockSession(true);
#if SERVO_COUNT >= 2
    servo2_.lock();
    state_.keep_window_locked_when_disarmed = true;
#endif
    mqttBus_.publishAck("lock all", true, "ok");
    return;
  }

  if (cmd == "unlock door") {
    servo1_.unlock();
    clearDoorUnlockSession(true);
    mqttBus_.publishAck("unlock door", true, "ok");
    return;
  }

#if SERVO_COUNT >= 2
  if (cmd == "unlock window") {
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    mqttBus_.publishAck("unlock window", true, "ok");
    return;
  }
#endif

  if (cmd == "unlock all") {
    servo1_.unlock();
    clearDoorUnlockSession(true);
#if SERVO_COUNT >= 2
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
#endif
    mqttBus_.publishAck("unlock all", true, "ok");
    return;
  }

  mqttBus_.publishAck("unknown", false, "unsupported command");
}

bool SecurityOrchestrator::processDoorHoldWarnSilenceEvent(const Event& e) {
  if (e.type != EventType::door_hold_warn_silence) return false;

  if (doorUnlockSessionActive_ && collector_.isDoorOpen() && doorHoldWarnActive_) {
    doorHoldWarnSilenced_ = true;
    buzzer_.stop();
    notifySvc_.send("door-open warning silenced");
  } else {
    Serial.println("[KEYPAD] silence ignored (not in door-open-hold warning)");
  }
  return true;
}

bool SecurityOrchestrator::processManualActuatorEvent(const Event& e) {
  if (e.type == EventType::manual_lock_request) {
    if (collector_.isDoorOpen()) {
      notifySvc_.send("manual lock rejected: door is open");
      return true;
    }
#if SERVO_COUNT >= 2
    if (collector_.isWindowOpen()) {
      notifySvc_.send("manual lock rejected: window is open");
      return true;
    }
    servo2_.lock();
    state_.keep_window_locked_when_disarmed = true;
#endif
    servo1_.lock();
    clearDoorUnlockSession(true);
    notifySvc_.send("manual lock accepted");
    return true;
  }

  if (e.type == EventType::manual_unlock_request) {
    servo1_.unlock();
    clearDoorUnlockSession(true);
#if SERVO_COUNT >= 2
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
#endif
    notifySvc_.send("manual unlock accepted");
    return true;
  }

  return false;
}

void SecurityOrchestrator::tick(uint32_t nowMs) {
  Event e;
  mqttBus_.update(nowMs);

  String remoteCmd;
  if (mqttBus_.pollCommand(remoteCmd)) processRemoteCommand(remoteCmd);

  if (collector_.pollKeypad(nowMs, e)) {
    if (processDoorHoldWarnSilenceEvent(e)) {
      updateDoorUnlockSession(nowMs);
      return;
    }
    if (EventGate::allowKeypadEvent(e)) {
      applyDecision(e);
      if (e.type == EventType::disarm) {
#if SERVO_COUNT >= 2
        state_.keep_window_locked_when_disarmed = true;
        servo2_.lock();
#endif
        startDoorUnlockSession(nowMs);
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

  buzzer_.update(nowMs);
  servo1_.update(nowMs);
#if SERVO_COUNT >= 2
  servo2_.update(nowMs);
#endif

  const bool hasEvent = collector_.pollSensorOrSerial(nowMs, e);
  updateDoorUnlockSession(nowMs);
  if (!hasEvent) return;
  if (processDoorHoldWarnSilenceEvent(e)) return;
  if (processManualActuatorEvent(e)) return;
  applyDecision(e);
}
