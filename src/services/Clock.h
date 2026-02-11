#pragma once
#include <Arduino.h>

class Clock {
public:
  void begin();
  uint32_t nowMs() const;
};
