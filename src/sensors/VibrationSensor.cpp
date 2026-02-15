#include "VibrationSensor.h"

VibrationSensor::VibrationSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0),
  last_active_(false)
{}

void VibrationSensor::begin() {
  pinMode(pin_, INPUT_PULLUP);
  last_fire_ms_ = 0;
  // INPUT_PULLUP means an open circuit reads HIGH. We only fire on a transition.
  last_active_ = (digitalRead(pin_) == HIGH);
}

bool VibrationSensor::poll(uint32_t nowMs, Event& out) {
  const bool active = (digitalRead(pin_) == HIGH);
  const bool rising_edge = (active && !last_active_);
  last_active_ = active;

  if (!rising_edge) return false;
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  last_fire_ms_ = nowMs;
  out = {EventType::vib_spike, nowMs, id_};
  return true;
}
