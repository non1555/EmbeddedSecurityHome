#include "ServoDriver.h"

ServoDriver::ServoDriver(uint8_t pin, uint8_t channel, uint8_t resolution_bits)
: pin_(pin), ch_(channel), res_(resolution_bits), min_us_(500), max_us_(2500) {}

void ServoDriver::begin() {
  ledcSetup(ch_, 50, res_);
  ledcAttachPin(pin_, ch_);
  writePulseUs(1500);
}

uint16_t ServoDriver::clampUs_(uint16_t us) const {
  if (us < min_us_) return min_us_;
  if (us > max_us_) return max_us_;
  return us;
}

void ServoDriver::writePulseUs(uint16_t us) {
  us = clampUs_(us);
  uint32_t maxDuty = (1u << res_) - 1u;
  uint32_t duty = (uint32_t)us * maxDuty / 20000u;
  ledcWrite(ch_, duty);
}

void ServoDriver::writeAngle(uint8_t deg) {
  if (deg > 180) deg = 180;
  uint32_t us = (uint32_t)min_us_ + ((uint32_t)(max_us_ - min_us_) * deg) / 180u;
  writePulseUs((uint16_t)us);
}
