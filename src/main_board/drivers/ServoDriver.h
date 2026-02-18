#pragma once
#include <Arduino.h>

class ServoDriver {
public:
  ServoDriver(uint8_t pin, uint8_t channel, uint8_t resolution_bits = 16);

  void begin();
  void writePulseUs(uint16_t us);
  void writeAngle(uint8_t deg);

private:
  uint8_t pin_;
  uint8_t ch_;
  uint8_t res_;
  uint16_t min_us_;
  uint16_t max_us_;

  uint16_t clampUs_(uint16_t us) const;
};
