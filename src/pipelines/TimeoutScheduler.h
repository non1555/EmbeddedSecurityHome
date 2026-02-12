#pragma once

#include <Arduino.h>

#include "app/Events.h"
#include "app/SystemState.h"

class TimeoutScheduler {
public:
  bool pollEntryTimeout(const SystemState& st, uint32_t nowMs, Event& out) const;
};

