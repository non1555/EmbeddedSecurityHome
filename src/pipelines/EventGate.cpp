#include "pipelines/EventGate.h"

#ifndef APP_ALLOW_KEYPAD_ARM
#define APP_ALLOW_KEYPAD_ARM 0
#endif

bool EventGate::allowKeypadEvent(const Event& e) {
  if (e.type == EventType::disarm) return true;
#if APP_ALLOW_KEYPAD_ARM
  if (e.type == EventType::arm_night || e.type == EventType::arm_away) return true;
#endif
  return false;
}

