#pragma once
#include <Arduino.h>
#include "app/Events.h"

class PirSensor {
public:
  PirSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms = 1500);

  void begin();
  bool poll(uint32_t nowMs, Event& out);
  bool isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const;

private:
  uint8_t pin_;
  uint8_t id_;
  uint32_t cooldown_ms_;
  uint32_t last_fire_ms_;
  bool last_active_;
  uint32_t active_since_ms_ = 0;
  bool seen_inactive_since_begin_ = false;
};
