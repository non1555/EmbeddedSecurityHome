#include "VibrationSensor.h"
#include "app/HardwareConfig.h"

VibrationSensor::VibrationSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0),
  last_active_(false)
{}

void VibrationSensor::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) {
    last_fire_ms_ = 0;
    last_active_ = false;
    active_since_ms_ = 0;
    return;
  }
  pinMode(pin_, INPUT_PULLUP);
  last_fire_ms_ = 0;
  // INPUT_PULLUP means an open circuit reads HIGH. We only fire on a transition.
  last_active_ = (digitalRead(pin_) == HIGH);
  active_since_ms_ = last_active_ ? millis() : 0;
}

bool VibrationSensor::poll(uint32_t nowMs, Event& out) {
  if (pin_ == HwCfg::PIN_UNUSED) return false;
  const bool active = (digitalRead(pin_) == HIGH);
  if (active && !last_active_) {
    active_since_ms_ = nowMs;
  } else if (!active) {
    active_since_ms_ = 0;
  }
  const bool rising_edge = (active && !last_active_);
  last_active_ = active;

  if (!rising_edge) return false;
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  last_fire_ms_ = nowMs;
  out = {EventType::vib_spike, nowMs, id_};
  return true;
}

bool VibrationSensor::isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const {
  if (!last_active_ || active_since_ms_ == 0 || thresholdMs == 0) return false;
  return (int32_t)(nowMs - (active_since_ms_ + thresholdMs)) >= 0;
}
