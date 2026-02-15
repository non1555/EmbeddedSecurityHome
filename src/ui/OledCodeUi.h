#pragma once

#include <Arduino.h>

class Adafruit_SSD1306;

class OledCodeUi {
public:
  OledCodeUi(uint8_t addr7, uint8_t w = 128, uint8_t h = 64);

  bool begin();
  void showCode(const char* code, uint8_t len);
  void showResult(bool ok);
  // Updates the door status line (does not change keypad/PIN UX).
  void setDoorStatus(bool doorLocked,
                     bool doorOpen,
                     bool countdownActive,
                     uint32_t countdownDeadlineMs,
                     uint32_t countdownWarnBeforeMs);
  void update(uint32_t nowMs);

private:
  uint8_t addr7_;
  uint8_t w_;
  uint8_t h_;

  // We allocate the display object at runtime to keep includes local to .cpp
  // (reduces compile impact on non-UI files).
  Adafruit_SSD1306* disp_;

  char code_[5];
  uint8_t len_;

  bool showing_result_;
  bool last_ok_;
  uint32_t result_until_ms_;

  // Door status line
  bool doorLocked_ = false;
  bool doorOpen_ = false;

  bool countdownActive_ = false;
  uint32_t countdownDeadlineMs_ = 0;
  uint32_t countdownWarnBeforeMs_ = 0;
  int lastCountdownSec_ = -1;
  bool lastCountdownUrgent_ = false;

  bool dirty_ = true;

  void render_();
};
