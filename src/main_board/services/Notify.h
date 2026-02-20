#pragma once
#include <Arduino.h>

class Notify {
public:
  void begin();
  void update(uint32_t nowMs);
  void setSerialEnabled(bool enabled);

  void send(const String& msg);

private:
  bool serialEnabled_ = false;
};
