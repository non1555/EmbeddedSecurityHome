#include "VibrationSensor.h"

VibrationSensor::VibrationSensor(uint8_t pin, uint8_t id, uint16_t threshold, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  threshold_(threshold),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0)
{}

void VibrationSensor::begin() {
  pinMode(pin_, INPUT);
  last_fire_ms_ = 0;
}

bool VibrationSensor::poll(uint32_t nowMs, Event& out) {
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  uint16_t v = readRaw_();
  if (v >= threshold_) {
    last_fire_ms_ = nowMs;
    out = {EventType::vib_spike, nowMs, id_};
    return true;
  }

  return false;
}

uint16_t VibrationSensor::readRaw_() const {
  return (uint16_t)analogRead(pin_);
}
