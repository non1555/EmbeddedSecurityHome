#pragma once
#include <Arduino.h>
#include "drivers/BuzzerDriver.h"

class Buzzer {
public:
  Buzzer(uint8_t pin, uint8_t channel = 0);

  void begin();
  void update(uint32_t nowMs);

  void warn();
  void alert();
  void stop();

  bool isActive() const;

private:
  enum class Mode : uint8_t { idle, warn, alert };

  BuzzerDriver drv_;
  Mode mode_;
  uint32_t next_ms_;
  uint8_t step_;
  bool tone_on_;

  void setTone_(bool on, uint32_t hz);
};
