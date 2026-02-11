#include "BuzzerDriver.h"

BuzzerDriver::BuzzerDriver(uint8_t pin, uint8_t channel, uint8_t resolution_bits)
: pin_(pin), ch_(channel), res_(resolution_bits), cur_hz_(0) {}

void BuzzerDriver::begin() {
  ledcSetup(ch_, 2000, res_);
  ledcAttachPin(pin_, ch_);
  ledcWrite(ch_, 0);
  cur_hz_ = 0;
}

void BuzzerDriver::startTone(uint32_t hz) {
  if (hz == 0) {
    stopTone();
    return;
  }
  if (cur_hz_ != hz) {
    ledcWriteTone(ch_, hz);
    cur_hz_ = hz;
  }
  uint32_t maxDuty = (1u << res_) - 1u;
  ledcWrite(ch_, maxDuty / 2u);
}

void BuzzerDriver::stopTone() {
  ledcWriteTone(ch_, 0);
  ledcWrite(ch_, 0);
  cur_hz_ = 0;
}
