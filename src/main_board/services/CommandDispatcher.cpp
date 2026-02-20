#include "CommandDispatcher.h"

static bool isDisarmed(const SystemState& st) {
  return st.mode == Mode::disarm;
}

void applyCommand(const Command& cmd, const SystemState& st, Actuators& acts, Notify* notify, Logger* logger) {
  (void)notify;
  if (st.mode == Mode::startup_safe) {
    if (acts.buzzer) acts.buzzer->stop();
    if (acts.servo1) acts.servo1->lock();
    if (acts.servo2) acts.servo2->lock();
  } else if (isDisarmed(st)) {
    if (acts.buzzer) acts.buzzer->stop();
    if (acts.servo2) {
      // In disarm mode, keep locks as-is unless policy explicitly requires window lock.
      if (st.keep_window_locked_when_disarmed) acts.servo2->lock();
    }
  } else {
    if (acts.servo1) acts.servo1->lock();
    if (acts.servo2) acts.servo2->lock();
  }

  switch (cmd.type) {
    case CommandType::buzzer_warn:
      if (acts.buzzer) acts.buzzer->warn();
      break;

    case CommandType::buzzer_alert:
      if (acts.buzzer) acts.buzzer->alert();
      break;

    case CommandType::servo_lock:
      if (acts.servo1) acts.servo1->lock();
      if (acts.servo2) acts.servo2->lock();
      break;

    case CommandType::none:
    default:
      break;
  }

  if (logger) logger->logCommand(cmd, st);
}
