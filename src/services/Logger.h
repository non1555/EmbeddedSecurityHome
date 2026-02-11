#pragma once
#include <Arduino.h>
#include "app/Commands.h"
#include "app/SystemState.h"

class Logger {
public:
  void begin();
  void update(uint32_t nowMs);

  void logCommand(const Command& cmd, const SystemState& st);
};
