#include "ChokepointSensor.h"

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
  inside_(false)
{}

void ChokepointSensor::begin() {
  next_sample_ms_ = 0;
  last_fire_ms_ = 0;
  last_cm_ = -1;
  inside_ = false;
}

int ChokepointSensor::lastCm() const {
  return last_cm_;
}

bool ChokepointSensor::poll(uint32_t nowMs, Event& out) {
  if (!drv_) return false;
  if (next_sample_ms_ != 0 && nowMs < next_sample_ms_) return false;

  next_sample_ms_ = nowMs + sample_period_ms_;

  int cm = drv_->readCm();
  last_cm_ = cm;

  if (cm < 0) return false;

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
