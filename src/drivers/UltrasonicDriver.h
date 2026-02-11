#pragma once
#include <Arduino.h>

class UltrasonicDriver {
public:
  UltrasonicDriver(uint8_t trigPin, uint8_t echoPin);

  void begin();

  // returns distance in cm, or -1 if timeout/no echo
  int readCm(uint32_t timeout_us = 25000);

private:
  uint8_t trig_;
  uint8_t echo_;
};
