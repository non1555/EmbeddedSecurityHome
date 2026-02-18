#pragma once

#include <stdint.h>

class ModeOverrideWindow {
public:
  void activate(uint32_t nowMs, uint32_t durationMs) {
    if (durationMs == 0) {
      clear();
      return;
    }
    active_ = true;
    untilMs_ = nowMs + durationMs;
  }

  void clear() {
    active_ = false;
    untilMs_ = 0;
  }

  bool active(uint32_t nowMs) {
    if (!active_) return false;
    if ((int32_t)(nowMs - untilMs_) < 0) return true;
    clear();
    return false;
  }

private:
  bool active_ = false;
  uint32_t untilMs_ = 0;
};

