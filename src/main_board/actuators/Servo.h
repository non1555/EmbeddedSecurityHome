#pragma once
#include <Arduino.h>
#include "drivers/ServoDriver.h"

class Servo {
public:
  Servo(uint8_t pin, uint8_t channel, uint8_t id, uint8_t lock_deg, uint8_t unlock_deg);

  void begin();
  void update(uint32_t nowMs);

  void lock();
  void unlock();

  bool isLocked() const;
  uint8_t id() const;

private:
  ServoDriver drv_;
  uint8_t id_;
  uint8_t lock_deg_;
  uint8_t unlock_deg_;

  uint8_t cur_deg_;
  uint8_t target_deg_;
  uint32_t next_ms_;

  void write_(uint8_t deg);
};
