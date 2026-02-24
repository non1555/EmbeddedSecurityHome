#include "hardware_abstraction_layer/DisplayManager.h"

#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "configuration_shared_types/Config.h"

namespace {

bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

bool beforeOrAt(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) <= 0;
}

uint32_t remainingMs(uint32_t nowMs, uint32_t targetMs) {
  const int32_t delta = (int32_t)(targetMs - nowMs);
  return (delta > 0) ? (uint32_t)delta : 0u;
}

} // namespace

namespace HardwareAbstractionLayer {

bool DisplayManager::begin() {
  if (display_) return true;

  display_ = new Adafruit_SSD1306(128, 64, &Wire, -1);
  if (!display_) return false;

  if (!display_->begin(SSD1306_SWITCHCAPVCC, ConfigurationSharedTypes::Config::OLED_I2C_ADDR)) {
    delete display_;
    display_ = nullptr;
    return false;
  }

  display_->clearDisplay();
  display_->setTextColor(SSD1306_WHITE);
  display_->setTextSize(1);
  display_->setCursor(0, 0);
  display_->println("EmbeddedSecurity");
  display_->println("Keypad ready");
  display_->display();
  delay(250);

  render_();
  return true;
}

void DisplayManager::showCode(const char* code, uint8_t length) {
  if (!display_) return;

  if (!code) code = "";
  if (length > 4) length = 4;

  codeLength_ = length;
  for (uint8_t i = 0; i < codeLength_; ++i) {
    code_[i] = code[i];
  }
  code_[codeLength_] = '\0';

  showingResult_ = false;
  resultUntilMs_ = 0;
  dirty_ = true;
  render_();
}

void DisplayManager::showSubmitResult(bool ok) {
  if (!display_) return;

  showingResult_ = true;
  resultOk_ = ok;
  resultUntilMs_ = millis() + 1200;
  dirty_ = true;
  render_();
}

void DisplayManager::updateDoorStatus(uint32_t nowMs,
                                      bool doorLocked,
                                      bool doorOpen,
                                      bool countdownActive,
                                      uint32_t countdownDeadlineMs,
                                      uint32_t countdownWarnBeforeMs) {
  if (!display_) return;

  bool changed = false;

  if (doorLocked_ != doorLocked) {
    doorLocked_ = doorLocked;
    changed = true;
  }

  if (doorOpen_ != doorOpen) {
    doorOpen_ = doorOpen;
    changed = true;
  }

  if (countdownActive_ != countdownActive) {
    countdownActive_ = countdownActive;
    changed = true;
  }

  if (countdownDeadlineMs_ != countdownDeadlineMs) {
    countdownDeadlineMs_ = countdownDeadlineMs;
    changed = true;
  }

  if (countdownWarnBeforeMs_ != countdownWarnBeforeMs) {
    countdownWarnBeforeMs_ = countdownWarnBeforeMs;
    changed = true;
  }

  if (changed) {
    dirty_ = true;
  }

  tick(nowMs);
}

void DisplayManager::tick(uint32_t nowMs) {
  if (!display_) return;

  if (showingResult_ && resultUntilMs_ != 0 && reached(nowMs, resultUntilMs_)) {
    showingResult_ = false;
    resultUntilMs_ = 0;
    dirty_ = true;
  }

  int secLeft = -1;
  bool urgent = false;
  if (countdownActive_ && countdownDeadlineMs_ != 0 && beforeOrAt(nowMs, countdownDeadlineMs_)) {
    const uint32_t msLeft = remainingMs(nowMs, countdownDeadlineMs_);
    secLeft = (int)((msLeft + 999u) / 1000u);
    urgent = (countdownWarnBeforeMs_ != 0) && ((uint32_t)secLeft * 1000u <= countdownWarnBeforeMs_);
  }

  if (dirty_ || secLeft != lastCountdownSec_ || urgent != lastCountdownUrgent_) {
    lastCountdownSec_ = secLeft;
    lastCountdownUrgent_ = urgent;
    render_();
  }
}

void DisplayManager::render_() {
  if (!display_) return;

  dirty_ = false;

  display_->clearDisplay();
  display_->setTextColor(SSD1306_WHITE);
  display_->setTextSize(1);
  display_->setCursor(0, 0);

  display_->print("DOOR: ");
  display_->print(doorLocked_ ? "LOCK" : "UNLOCK");
  if (doorOpen_) {
    display_->print(" OPEN");
  }

  const uint32_t nowMs = millis();
  if (countdownActive_ && countdownDeadlineMs_ != 0 && beforeOrAt(nowMs, countdownDeadlineMs_)) {
    const uint32_t msLeft = remainingMs(nowMs, countdownDeadlineMs_);
    const uint32_t secLeft = (msLeft + 999u) / 1000u;
    display_->print(" ");
    display_->print(secLeft);
    display_->print("s");

    const bool urgent = (countdownWarnBeforeMs_ != 0) && (secLeft * 1000u <= countdownWarnBeforeMs_);
    if (urgent) {
      display_->print("!");
    }
  }
  display_->println();

  display_->println("PIN:");

  display_->setTextSize(2);
  display_->setCursor(0, 16);
  if (codeLength_ == 0) {
    display_->println("____");
  } else {
    display_->print(code_);
    for (uint8_t i = codeLength_; i < 4; ++i) {
      display_->print('_');
    }
    display_->println();
  }

  display_->setTextSize(2);
  display_->setCursor(0, 44);
  if (showingResult_) {
    display_->print(resultOk_ ? "OK" : "ERR");
  } else {
    display_->print("    ");
  }

  display_->display();
}

} // namespace HardwareAbstractionLayer
