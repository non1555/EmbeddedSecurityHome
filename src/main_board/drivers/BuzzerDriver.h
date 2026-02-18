#pragma once
#include <Arduino.h>

class BuzzerDriver {
public:
  BuzzerDriver(uint8_t pin, uint8_t channel = 0, uint8_t resolution_bits = 10);

  void begin();
  void startTone(uint32_t hz);
  void stopTone();

private:
  uint8_t pin_;
  uint8_t ch_;
  uint8_t res_;
  uint32_t cur_hz_;
};
