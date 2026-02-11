#pragma once
#include <Arduino.h>
#include "app/Events.h"

class VibrationSensor {
public:
  VibrationSensor(uint8_t pin, uint8_t id, uint16_t threshold = 600, uint32_t cooldown_ms = 500);

  void begin();
  bool poll(uint32_t nowMs, Event& out);

private:
  uint8_t pin_;
  uint8_t id_;
  uint16_t threshold_;
  uint32_t cooldown_ms_;
  uint32_t last_fire_ms_;

  uint16_t readRaw_() const;
};
