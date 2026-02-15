#include "pipelines/EventGate.h"

bool EventGate::allowKeypadEvent(const Event& e) {
  if (e.type == EventType::disarm) return true;
  return false;
}
