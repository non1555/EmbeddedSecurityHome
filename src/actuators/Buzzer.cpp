#include "Buzzer.h"

Buzzer::Buzzer(uint8_t pin, uint8_t channel)
: drv_(pin, channel), mode_(Mode::idle), next_ms_(0), step_(0), tone_on_(false) {}

void Buzzer::begin() {
  drv_.begin();
  stop();
}

void Buzzer::setTone_(bool on, uint32_t hz) {
  if (on) drv_.startTone(hz);
  else drv_.stopTone();
  tone_on_ = on;
}

void Buzzer::warn() {
  mode_ = Mode::warn;
  step_ = 0;
  next_ms_ = 0;
  tone_on_ = false;
}

void Buzzer::alert() {
  mode_ = Mode::alert;
  step_ = 0;
  next_ms_ = 0;
  tone_on_ = false;
}

void Buzzer::stop() {
  mode_ = Mode::idle;
  step_ = 0;
  next_ms_ = 0;
  setTone_(false, 0);
}

bool Buzzer::isActive() const {
  return mode_ != Mode::idle;
}

void Buzzer::update(uint32_t nowMs) {
  if (mode_ == Mode::idle) return;
  if (next_ms_ != 0 && nowMs < next_ms_) return;

  if (mode_ == Mode::warn) {
    const uint32_t hz = 2200;
    if (!tone_on_) {
      setTone_(true, hz);
      next_ms_ = nowMs + 180;
    } else {
      setTone_(false, 0);
      next_ms_ = nowMs + 220;
      step_++;
      if (step_ >= 6) stop();
    }
    return;
  }

  if (mode_ == Mode::alert) {
    const uint32_t hz = 3200;
    if (!tone_on_) {
      setTone_(true, hz);
      next_ms_ = nowMs + 200;
    } else {
      setTone_(false, 0);
      next_ms_ = nowMs + 120;
    }
    return;
  }
}
