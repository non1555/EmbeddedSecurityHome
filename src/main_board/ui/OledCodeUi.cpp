#include "ui/OledCodeUi.h"

#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

inline bool beforeOrAt(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) <= 0;
}

inline uint32_t remainingMs(uint32_t nowMs, uint32_t targetMs) {
  const int32_t delta = (int32_t)(targetMs - nowMs);
  return (delta > 0) ? (uint32_t)delta : 0u;
}
} // namespace

OledCodeUi::OledCodeUi(uint8_t addr7, uint8_t w, uint8_t h)
: addr7_(addr7),
  w_(w),
  h_(h),
  disp_(nullptr),
  len_(0),
  showing_result_(false),
  last_ok_(false),
  result_until_ms_(0) {
  code_[0] = '\0';
}

bool OledCodeUi::begin() {
  if (disp_) return true;

  // -1 reset pin: common I2C modules omit reset.
  disp_ = new Adafruit_SSD1306(w_, h_, &Wire, -1);
  if (!disp_) return false;

  // Use explicit I2C address.
  if (!disp_->begin(SSD1306_SWITCHCAPVCC, addr7_)) {
    delete disp_;
    disp_ = nullptr;
    return false;
  }

  disp_->clearDisplay();
  disp_->setTextColor(SSD1306_WHITE);
  disp_->setTextSize(1);
  disp_->setCursor(0, 0);
  disp_->println("EmbeddedSecurity");
  disp_->println("Keypad ready");
  disp_->display();
  delay(250);

  render_();
  return true;
}

void OledCodeUi::showCode(const char* code, uint8_t len) {
  if (!disp_) return;
  if (!code) code = "";
  if (len > 4) len = 4;

  len_ = len;
  for (uint8_t i = 0; i < len_; ++i) {
    code_[i] = code[i];
  }
  code_[len_] = '\0';

  // If user is typing, hide result screen.
  showing_result_ = false;
  result_until_ms_ = 0;
  dirty_ = true;
  render_();
}

void OledCodeUi::showResult(bool ok) {
  if (!disp_) return;
  showing_result_ = true;
  last_ok_ = ok;
  result_until_ms_ = millis() + 1200;
  dirty_ = true;
  render_();
}

void OledCodeUi::setDoorStatus(bool doorLocked,
                               bool doorOpen,
                               bool countdownActive,
                               uint32_t countdownDeadlineMs,
                               uint32_t countdownWarnBeforeMs) {
  if (!disp_) return;

  bool changed = false;
  if (doorLocked_ != doorLocked) { doorLocked_ = doorLocked; changed = true; }
  if (doorOpen_ != doorOpen) { doorOpen_ = doorOpen; changed = true; }

  if (countdownActive_ != countdownActive) { countdownActive_ = countdownActive; changed = true; }
  if (countdownDeadlineMs_ != countdownDeadlineMs) { countdownDeadlineMs_ = countdownDeadlineMs; changed = true; }
  if (countdownWarnBeforeMs_ != countdownWarnBeforeMs) { countdownWarnBeforeMs_ = countdownWarnBeforeMs; changed = true; }

  if (changed) {
    dirty_ = true;
    render_();
  }
}

void OledCodeUi::update(uint32_t nowMs) {
  if (!disp_) return;
  if (showing_result_ && result_until_ms_ != 0 && reached(nowMs, result_until_ms_)) {
    showing_result_ = false;
    result_until_ms_ = 0;
    dirty_ = true;
    render_();
    return;
  }

  // Re-render when countdown display should change (once per second).
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

void OledCodeUi::render_() {
  if (!disp_) return;

  dirty_ = false;

  disp_->clearDisplay();
  disp_->setTextColor(SSD1306_WHITE);
  disp_->setTextSize(1);
  disp_->setCursor(0, 0);
  disp_->print("DOOR: ");
  disp_->print(doorLocked_ ? "LOCK" : "UNLOCK");
  if (doorOpen_) disp_->print(" OPEN");

  const uint32_t nowMs = millis();
  if (countdownActive_ && countdownDeadlineMs_ != 0 && beforeOrAt(nowMs, countdownDeadlineMs_)) {
    const uint32_t msLeft = remainingMs(nowMs, countdownDeadlineMs_);
    const uint32_t secLeft = (msLeft + 999u) / 1000u;
    disp_->print(" ");
    disp_->print(secLeft);
    disp_->print("s");
    const bool urgent = (countdownWarnBeforeMs_ != 0) && (secLeft * 1000u <= countdownWarnBeforeMs_);
    if (urgent) disp_->print("!");
  }
  disp_->println();

  disp_->println("PIN:");

  disp_->setTextSize(2);
  disp_->setCursor(0, 16);
  if (len_ == 0) {
    disp_->println("____");
  } else {
    // Show the digits as entered (per request).
    disp_->print(code_);
    for (uint8_t i = len_; i < 4; ++i) disp_->print('_');
    disp_->println();
  }

  disp_->setTextSize(2);
  disp_->setCursor(0, 44);
  if (showing_result_) {
    disp_->print(last_ok_ ? "OK" : "ERR");
  } else {
    disp_->print("    ");
  }

  disp_->display();
}
