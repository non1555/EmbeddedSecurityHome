#pragma once

#include <Arduino.h>

class Adafruit_SSD1306;

namespace HardwareAbstractionLayer {

class DisplayManager {
public:
  bool begin(); // Initializes OLED display and splash content. Params: none.
  void showCode(const char* code, uint8_t length); // Updates entered PIN text on OLED. Params: code=PIN buffer text, length=number of valid characters.
  void showSubmitResult(bool ok); // Shows temporary submit result (OK/ERR). Params: ok=true for success, false for failure.
  void updateDoorStatus(uint32_t nowMs,
                        bool doorLocked,
                        bool doorOpen,
                        bool countdownActive,
                        uint32_t countdownDeadlineMs,
                        uint32_t countdownWarnBeforeMs); // Updates cached door/countdown state and refreshes display. Params: nowMs=current timestamp in ms, doorLocked=door lock status, doorOpen=door open status, countdownActive=countdown active flag, countdownDeadlineMs=countdown end time, countdownWarnBeforeMs=warning threshold.
  void tick(uint32_t nowMs); // Performs timed UI updates (countdown/result timeout). Params: nowMs=current timestamp in ms.

private:
  Adafruit_SSD1306* display_ = nullptr;

  char code_[5] = {0, 0, 0, 0, 0};
  uint8_t codeLength_ = 0;

  bool showingResult_ = false;
  bool resultOk_ = false;
  uint32_t resultUntilMs_ = 0;

  bool doorLocked_ = false;
  bool doorOpen_ = false;
  bool countdownActive_ = false;
  uint32_t countdownDeadlineMs_ = 0;
  uint32_t countdownWarnBeforeMs_ = 0;

  int lastCountdownSec_ = -1;
  bool lastCountdownUrgent_ = false;
  bool dirty_ = true;

  void render_(); // Renders complete OLED frame from cached UI state. Params: none.
};

} // namespace HardwareAbstractionLayer
