#pragma once

#include "app/Events.h"

class EventGate {
public:
  static bool allowKeypadEvent(const Event& e);
};

