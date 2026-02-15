#include "CommandDispatcher.h"

static bool isDisarmed(const SystemState& st) {
  return st.mode == Mode::disarm;
}

static const char* levelText(const SystemState& st) {
  int lv = (int)st.level;
  if (lv <= 0) return "none";
  if (lv == 1) return "warn";
  if (lv == 2) return "alert";
  return "critical";
}

void applyCommand(const Command& cmd, const SystemState& st, Actuators& acts, Notify* notify, Logger* logger) {
  if (isDisarmed(st)) {
    if (acts.buzzer) acts.buzzer->stop();
    if (acts.servo1) acts.servo1->unlock();
    if (acts.servo2) {
      if (st.keep_window_locked_when_disarmed) acts.servo2->lock();
      else acts.servo2->unlock();
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

    case CommandType::notify:
      if (notify) {
        String msg = String("alarm=") + levelText(st) + " mode=" + String((int)st.mode);
        notify->send(msg);
      }
      // When escalation reaches alert/critical, make sure buzzer is audible even if we choose to notify.
      if (acts.buzzer && (st.level == AlarmLevel::alert || st.level == AlarmLevel::critical)) {
        acts.buzzer->alert();
      }
      break;

    case CommandType::none:
    default:
      break;
  }

  if (logger) logger->logCommand(cmd, st);
}
