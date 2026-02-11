#pragma once
#include <Arduino.h>
#include "app/Events.h"

class ReedSensor {
public:
  ReedSensor(uint8_t pin, uint8_t id, bool open_is_high = true, uint32_t debounce_ms = 80);

  void begin();
  bool poll(uint32_t nowMs, Event& out);

  bool isOpen() const;

private:
  uint8_t pin_;
  uint8_t id_;
  bool open_is_high_;
  uint32_t debounce_ms_;

  bool stable_open_;
  bool last_raw_;
  uint32_t last_flip_ms_;
  bool fired_open_;

  bool readOpenRaw_() const;
};
