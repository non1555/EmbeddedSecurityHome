#pragma once
#include <Arduino.h>
#include "app/Events.h"
#include "drivers/UltrasonicDriver.h"

class ChokepointSensor {
public:
  ChokepointSensor(UltrasonicDriver* drv, uint8_t id,
                   int near_cm = 35,
                   int far_cm = 55,
                   uint32_t sample_period_ms = 200,
                   uint32_t cooldown_ms = 1500);

  void begin();
  bool poll(uint32_t nowMs, Event& out);

  int lastCm() const;
  bool isOffline(uint32_t nowMs, uint32_t noValidMs, uint16_t noEchoCount) const;
  uint16_t consecutiveNoEcho() const;
  uint32_t lastValidMs() const;

private:
  UltrasonicDriver* drv_;
  uint8_t id_;

  int near_cm_;
  int far_cm_;
  uint32_t sample_period_ms_;
  uint32_t cooldown_ms_;

  uint32_t next_sample_ms_;
  uint32_t last_fire_ms_;

  int last_cm_;
  bool inside_;
  uint16_t consecutive_no_echo_;
  uint32_t last_valid_ms_;
};
