#include "presence.h"

bool isSomeoneHome = true;

namespace Presence {
namespace {

enum class EntryStage : uint8_t {
  idle = 0,
  unlocked_wait_ultrasonic,
  ultrasonic_wait_pir,
};

enum class ExitStage : uint8_t {
  idle = 0,
  saw_ultrasonic,
  saw_door_open,
  door_closed_wait_no_pir,
};

Config cfg_;
State state_ = State::unknown;

EntryStage entryStage_ = EntryStage::idle;
uint32_t entryDeadlineMs_ = 0;

ExitStage exitStage_ = ExitStage::idle;
uint32_t exitDeadlineMs_ = 0;

uint32_t lastAwayAtMs_ = 0;
uint32_t lastPirMs_ = 0;

inline bool deadlinePassed(uint32_t nowMs, uint32_t deadlineMs) {
  return (int32_t)(nowMs - deadlineMs) >= 0;
}

inline bool beforeOrAt(uint32_t nowMs, uint32_t deadlineMs) {
  return (int32_t)(nowMs - deadlineMs) <= 0;
}

void setState(State s, uint32_t nowMs) {
  state_ = s;
  isSomeoneHome = (s == State::home);
  if (s == State::away) {
    lastAwayAtMs_ = nowMs;
  }
}

void resetEntry() {
  entryStage_ = EntryStage::idle;
  entryDeadlineMs_ = 0;
}

void resetExit() {
  exitStage_ = ExitStage::idle;
  exitDeadlineMs_ = 0;
}

} // namespace

void init(const Config& cfg) {
  cfg_ = cfg;
  state_ = State::unknown;
  isSomeoneHome = true;
  resetEntry();
  resetExit();
  lastAwayAtMs_ = 0;
  lastPirMs_ = 0;
}

void onDoorUnlock(uint32_t nowMs) {
  entryStage_ = EntryStage::unlocked_wait_ultrasonic;
  entryDeadlineMs_ = nowMs + cfg_.unlock_to_ultrasonic_ms;
}

void onDoorUltrasonic(uint32_t nowMs) {
  // Entry flow: unlock -> ultrasonic -> pir.
  if (entryStage_ == EntryStage::unlocked_wait_ultrasonic && beforeOrAt(nowMs, entryDeadlineMs_)) {
    entryStage_ = EntryStage::ultrasonic_wait_pir;
    entryDeadlineMs_ = nowMs + cfg_.entry_pir_ms;
  }

  // Exit flow starts from inside ultrasonic.
  exitStage_ = ExitStage::saw_ultrasonic;
  exitDeadlineMs_ = nowMs + cfg_.exit_sequence_ms;
}

void onDoorOpen(uint32_t nowMs) {
  if (exitStage_ == ExitStage::saw_ultrasonic && beforeOrAt(nowMs, exitDeadlineMs_)) {
    exitStage_ = ExitStage::saw_door_open;
    exitDeadlineMs_ = nowMs + cfg_.exit_sequence_ms;
  }
}

void onDoorClose(uint32_t nowMs) {
  if (exitStage_ == ExitStage::saw_door_open && beforeOrAt(nowMs, exitDeadlineMs_)) {
    exitStage_ = ExitStage::door_closed_wait_no_pir;
    exitDeadlineMs_ = nowMs + cfg_.away_no_pir_ms;
  }
}

void onPirDetected(uint32_t nowMs) {
  lastPirMs_ = nowMs;

  if (entryStage_ == EntryStage::ultrasonic_wait_pir && beforeOrAt(nowMs, entryDeadlineMs_)) {
    setState(State::home, nowMs);
    resetEntry();
    resetExit();
    return;
  }

  // If we were waiting to confirm away, PIR cancels that.
  if (exitStage_ == ExitStage::door_closed_wait_no_pir) {
    setState(State::home, nowMs);
    resetExit();
    return;
  }

  // Grace revert: shortly after away, PIR means still occupied.
  if (state_ == State::away &&
      lastAwayAtMs_ != 0 &&
      !deadlinePassed(nowMs, lastAwayAtMs_ + cfg_.away_revert_pir_ms)) {
    setState(State::home, nowMs);
    resetExit();
  }
}

void tick(uint32_t nowMs) {
  if (entryStage_ != EntryStage::idle && deadlinePassed(nowMs, entryDeadlineMs_)) {
    resetEntry();
  }

  if (exitStage_ != ExitStage::idle && deadlinePassed(nowMs, exitDeadlineMs_)) {
    if (exitStage_ == ExitStage::door_closed_wait_no_pir) {
      setState(State::away, nowMs);
    }
    resetExit();
  }
}

void setExternalHome(bool home, uint32_t nowMs) {
  setState(home ? State::home : State::away, nowMs);
  resetEntry();
  resetExit();
}

State state() {
  return state_;
}

bool isHome() {
  return state_ == State::home;
}

} // namespace Presence
