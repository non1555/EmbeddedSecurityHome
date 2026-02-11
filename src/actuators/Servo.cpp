#include "Servo.h"

Servo::Servo(uint8_t pin, uint8_t channel, uint8_t id, uint8_t lock_deg, uint8_t unlock_deg)
: drv_(pin, channel),
  id_(id),
  lock_deg_(lock_deg),
  unlock_deg_(unlock_deg),
  cur_deg_(unlock_deg),
  target_deg_(unlock_deg),
  next_ms_(0) {}

void Servo::write_(uint8_t deg) {
  drv_.writeAngle(deg);
  cur_deg_ = deg;
}

void Servo::begin() {
  drv_.begin();
  write_(unlock_deg_);
  target_deg_ = unlock_deg_;
  next_ms_ = 0;
}

void Servo::lock() {
  target_deg_ = lock_deg_;
  next_ms_ = 0;
}

void Servo::unlock() {
  target_deg_ = unlock_deg_;
  next_ms_ = 0;
}

bool Servo::isLocked() const {
  return cur_deg_ == lock_deg_;
}

uint8_t Servo::id() const {
  return id_;
}

void Servo::update(uint32_t nowMs) {
  if (cur_deg_ == target_deg_) return;
  if (next_ms_ != 0 && nowMs < next_ms_) return;

  if (cur_deg_ < target_deg_) {
    cur_deg_++;
  } else {
    cur_deg_--;
  }

  drv_.writeAngle(cur_deg_);
  next_ms_ = nowMs + 15;
}
