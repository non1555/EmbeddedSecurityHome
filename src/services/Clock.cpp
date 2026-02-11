#include "Clock.h"

void Clock::begin() {}

uint32_t Clock::nowMs() const {
  return millis();
}
