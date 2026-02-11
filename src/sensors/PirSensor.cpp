#include "PirSensor.h"

PirSensor::PirSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0)
{}

void PirSensor::begin() {
  pinMode(pin_, INPUT);
  last_fire_ms_ = 0;
}

bool PirSensor::poll(uint32_t nowMs, Event& out) {
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  int v = digitalRead(pin_);
  if (v == HIGH) {
    last_fire_ms_ = nowMs;
    out = {EventType::motion, nowMs, id_};
    return true;
  }
  return false;
}
