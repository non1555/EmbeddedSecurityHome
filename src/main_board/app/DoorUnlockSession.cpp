#include "app/DoorUnlockSession.h"

namespace {
inline bool reached(uint32_t nowMs, uint32_t deadlineMs) {
  return (int32_t)(nowMs - deadlineMs) >= 0;
}

inline bool before(uint32_t nowMs, uint32_t deadlineMs) {
  return (int32_t)(nowMs - deadlineMs) < 0;
}
} // namespace

void DoorUnlockSession::start(uint32_t nowMs, bool doorOpen, const Config& cfg) {
  active_ = true;
  sawOpen_ = doorOpen;
  holdWarnActive_ = false;
  holdWarnSilenced_ = false;
  unlockDeadlineMs_ = nowMs + cfg.door_unlock_timeout_ms;
  doorWasOpenLastTick_ = doorOpen;
  openWarnAtMs_ = doorOpen ? (nowMs + cfg.door_open_hold_warn_after_ms) : 0;
  closeLockAtMs_ = 0;
  nextWarnMs_ = 0;
}

void DoorUnlockSession::clear(bool stopBuzzer, Buzzer& buzzer) {
  active_ = false;
  sawOpen_ = false;
  holdWarnActive_ = false;
  holdWarnSilenced_ = false;
  unlockDeadlineMs_ = 0;
  openWarnAtMs_ = 0;
  closeLockAtMs_ = 0;
  nextWarnMs_ = 0;
  if (stopBuzzer) buzzer.stop();
}

void DoorUnlockSession::update(uint32_t nowMs,
                               bool doorOpen,
                               const Config& cfg,
                               Servo& doorServo,
                               Buzzer& buzzer,
                               Notify& notify) {
  if (!active_) return;

  static constexpr uint32_t kAutoLockAfterCloseMs = 3000;

  if (!doorWasOpenLastTick_ && doorOpen) {
    sawOpen_ = true;
    holdWarnActive_ = false;
    holdWarnSilenced_ = false;
    openWarnAtMs_ = nowMs + cfg.door_open_hold_warn_after_ms;
    closeLockAtMs_ = 0;
    nextWarnMs_ = 0;
  }

  if (doorWasOpenLastTick_ && !doorOpen) {
    holdWarnActive_ = false;
    holdWarnSilenced_ = false;
    openWarnAtMs_ = 0;
    closeLockAtMs_ = nowMs + kAutoLockAfterCloseMs;
    nextWarnMs_ = 0;
  }
  doorWasOpenLastTick_ = doorOpen;

  if (closeLockAtMs_ != 0) {
    if (doorOpen) {
      closeLockAtMs_ = 0;
    } else if (reached(nowMs, closeLockAtMs_)) {
      doorServo.lock();
      notify.send("door auto-locked after close");
      clear(true, buzzer);
    }
    return;
  }

  if (!sawOpen_) {
    if (reached(nowMs, unlockDeadlineMs_)) {
      doorServo.lock();
      notify.send("door auto-locked: unlock timeout");
      clear(true, buzzer);
      return;
    }

    const uint32_t timeLeftMs = unlockDeadlineMs_ - nowMs;
    if (timeLeftMs <= cfg.door_unlock_warn_before_ms &&
        (nextWarnMs_ == 0 || reached(nowMs, nextWarnMs_))) {
      buzzer.warn();
      nextWarnMs_ = nowMs + cfg.door_warn_retrigger_ms;
    }
    return;
  }

  if (doorOpen && openWarnAtMs_ != 0 && reached(nowMs, openWarnAtMs_)) {
    holdWarnActive_ = true;
    if (!holdWarnSilenced_ && (nextWarnMs_ == 0 || reached(nowMs, nextWarnMs_))) {
      buzzer.warn();
      nextWarnMs_ = nowMs + cfg.door_warn_retrigger_ms;
    }
  }
}

bool DoorUnlockSession::silenceHoldWarning(bool doorOpen, Buzzer& buzzer, Notify& notify) {
  if (!(active_ && doorOpen && holdWarnActive_)) return false;
  holdWarnSilenced_ = true;
  buzzer.stop();
  notify.send("door-open warning silenced");
  return true;
}

bool DoorUnlockSession::countdown(uint32_t nowMs,
                                  bool doorLocked,
                                  bool doorOpen,
                                  const Config& cfg,
                                  uint32_t& deadlineMs,
                                  uint32_t& warnBeforeMs) const {
  deadlineMs = 0;
  warnBeforeMs = 0;
  if (!active_ || doorLocked) return false;

  if (!sawOpen_) {
    deadlineMs = unlockDeadlineMs_;
    warnBeforeMs = cfg.door_unlock_warn_before_ms;
    return (deadlineMs != 0 && before(nowMs, deadlineMs));
  }
  if (doorOpen) {
    deadlineMs = openWarnAtMs_;
    warnBeforeMs = 2000;
    return (deadlineMs != 0 && before(nowMs, deadlineMs));
  }
  if (closeLockAtMs_ != 0) {
    deadlineMs = closeLockAtMs_;
    warnBeforeMs = 1000;
    return before(nowMs, deadlineMs);
  }
  return false;
}

bool DoorUnlockSession::isActive() const {
  return active_;
}
