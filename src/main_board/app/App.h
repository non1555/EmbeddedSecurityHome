#pragma once
#include <Arduino.h>

class App {
public:
  void begin();
  void tick(uint32_t nowMs);
};
