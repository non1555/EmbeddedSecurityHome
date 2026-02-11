#include "ReedSensor.h"

ReedSensor::ReedSensor(uint8_t pin, uint8_t id, bool open_is_high, uint32_t debounce_ms)
: pin_(pin),
  id_(id),
  open_is_high_(open_is_high),
  debounce_ms_(debounce_ms),
  stable_open_(false),
  last_raw_(false),
  last_flip_ms_(0),
  fired_open_(false)
{}

void ReedSensor::begin() {
  pinMode(pin_, INPUT_PULLUP);
  stable_open_ = readOpenRaw_();
  last_raw_ = stable_open_;
  last_flip_ms_ = millis();
  fired_open_ = false;
}

bool ReedSensor::poll(uint32_t nowMs, Event& out) {
  bool raw = readOpenRaw_();

  if (raw != last_raw_) {
    last_raw_ = raw;
    last_flip_ms_ = nowMs;
  }

  if ((nowMs - last_flip_ms_) < debounce_ms_) return false;

  if (stable_open_ != raw) {
    stable_open_ = raw;
    if (stable_open_) fired_open_ = false;
  }

  if (stable_open_ && !fired_open_) {
    fired_open_ = true;
    out = {EventType::window_open, nowMs, id_};
    return true;
  }

  return false;
}

bool ReedSensor::isOpen() const {
  return stable_open_;
}

bool ReedSensor::readOpenRaw_() const {
  int v = digitalRead(pin_);
  bool high = (v == HIGH);
  return open_is_high_ ? high : !high;
}
