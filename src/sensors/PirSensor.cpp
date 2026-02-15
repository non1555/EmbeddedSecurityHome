#include "PirSensor.h"

PirSensor::PirSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0),
  last_active_(false)
{}

void PirSensor::begin() {
  pinMode(pin_, INPUT);
  last_fire_ms_ = 0;
  last_active_ = (digitalRead(pin_) == HIGH);
}

bool PirSensor::poll(uint32_t nowMs, Event& out) {
  const bool active = (digitalRead(pin_) == HIGH);
  const bool rising_edge = (active && !last_active_);
  last_active_ = active;

  if (!rising_edge) return false;
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  last_fire_ms_ = nowMs;
  out = {EventType::motion, nowMs, id_};
  return true;
}
