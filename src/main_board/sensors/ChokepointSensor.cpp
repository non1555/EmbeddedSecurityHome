#include "ChokepointSensor.h"

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}
} // namespace

ChokepointSensor::ChokepointSensor(UltrasonicDriver* drv, uint8_t id,
                                   int near_cm, int far_cm,
                                   uint32_t sample_period_ms, uint32_t cooldown_ms)
: drv_(drv),
  id_(id),
  near_cm_(near_cm),
  far_cm_(far_cm),
  sample_period_ms_(sample_period_ms),
  cooldown_ms_(cooldown_ms),
  next_sample_ms_(0),
  last_fire_ms_(0),
  last_cm_(-1),
  inside_(false),
  consecutive_no_echo_(0),
  last_valid_ms_(0)
{}

void ChokepointSensor::begin() {
  next_sample_ms_ = 0;
  last_fire_ms_ = 0;
  last_cm_ = -1;
  inside_ = false;
  consecutive_no_echo_ = 0;
  last_valid_ms_ = 0;
}

int ChokepointSensor::lastCm() const {
  return last_cm_;
}

bool ChokepointSensor::poll(uint32_t nowMs, Event& out) {
  if (!drv_) return false;
  if (next_sample_ms_ != 0 && !reached(nowMs, next_sample_ms_)) return false;

  next_sample_ms_ = nowMs + sample_period_ms_;

  int cm = drv_->readCm();
  last_cm_ = cm;

  if (cm < 0) {
    if (consecutive_no_echo_ < 0xFFFFu) consecutive_no_echo_++;
    return false;
  }
  consecutive_no_echo_ = 0;
  last_valid_ms_ = nowMs;

  // hysteresis: enter when <= near, exit when >= far
  if (!inside_) {
    if (cm <= near_cm_) {
      inside_ = true;

      if ((nowMs - last_fire_ms_) >= cooldown_ms_) {
        last_fire_ms_ = nowMs;
        out = {EventType::chokepoint, nowMs, id_};
        return true;
      }
    }
  } else {
    if (cm >= far_cm_) {
      inside_ = false;
    }
  }

  return false;
}

bool ChokepointSensor::isOffline(uint32_t nowMs, uint32_t noValidMs, uint16_t noEchoCount) const {
  const bool noEchoTooMany = (noEchoCount > 0 && consecutive_no_echo_ >= noEchoCount);
  if (noValidMs == 0) return noEchoTooMany;

  bool noValidTooLong = false;
  if (last_valid_ms_ == 0) {
    noValidTooLong = (int32_t)(nowMs - noValidMs) >= 0;
  } else {
    noValidTooLong = (int32_t)(nowMs - (last_valid_ms_ + noValidMs)) >= 0;
  }
  return noEchoTooMany || noValidTooLong;
}

uint16_t ChokepointSensor::consecutiveNoEcho() const {
  return consecutive_no_echo_;
}

uint32_t ChokepointSensor::lastValidMs() const {
  return last_valid_ms_;
}
