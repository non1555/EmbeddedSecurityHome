#pragma once
#include <Arduino.h>

class Notify {
public:
  void begin();
  void update(uint32_t nowMs);

  void send(const String& msg);
};
