#include "pipelines/EventGate.h"

bool EventGate::allowKeypadEvent(const Event& e) {
  return e.type == EventType::disarm ||
         e.type == EventType::arm_night ||
         e.type == EventType::arm_away;
}
