#include "PirSensor.h"
#include "app/HardwareConfig.h"

PirSensor::PirSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0),
  last_active_(false),
  seen_inactive_since_begin_(false)
{}

void PirSensor::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) {
    last_fire_ms_ = 0;
    last_active_ = false;
    active_since_ms_ = 0;
    seen_inactive_since_begin_ = true;
    return;
  }
  pinMode(pin_, INPUT);
  last_fire_ms_ = 0;
  last_active_ = (digitalRead(pin_) == HIGH);
  active_since_ms_ = last_active_ ? millis() : 0;
  // Require at least one observed inactive sample before flagging stuck-active.
  // This prevents false sensor-faults when a channel boots floating/high.
  seen_inactive_since_begin_ = !last_active_;
}

bool PirSensor::poll(uint32_t nowMs, Event& out) {
  if (pin_ == HwCfg::PIN_UNUSED) return false;
  const bool active = (digitalRead(pin_) == HIGH);
  if (active && !last_active_) {
    active_since_ms_ = nowMs;
  } else if (!active) {
    active_since_ms_ = 0;
    seen_inactive_since_begin_ = true;
  }
  const bool rising_edge = (active && !last_active_);
  last_active_ = active;

  if (!rising_edge) return false;
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  last_fire_ms_ = nowMs;
  out = {EventType::motion, nowMs, id_};
  return true;
}

bool PirSensor::isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const {
  if (!seen_inactive_since_begin_ || !last_active_ || active_since_ms_ == 0 || thresholdMs == 0) return false;
  return (int32_t)(nowMs - (active_since_ms_ + thresholdMs)) >= 0;
}
