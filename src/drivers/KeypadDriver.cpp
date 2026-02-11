#include "KeypadDriver.h"

KeypadDriver::KeypadDriver(const uint8_t* rows, uint8_t nRows,
                           const uint8_t* cols, uint8_t nCols,
                           const char* keymap, uint32_t debounce_ms)
: rows_(rows), cols_(cols), nRows_(nRows), nCols_(nCols),
  keymap_(keymap), debounce_ms_(debounce_ms),
  scanRow_(0), waitingRelease_(false), lastKey_(0), lastKeyMs_(0) {}

void KeypadDriver::begin() {
  for (uint8_t r = 0; r < nRows_; r++) {
    pinMode(rows_[r], OUTPUT);
    digitalWrite(rows_[r], HIGH);
  }
  for (uint8_t c = 0; c < nCols_; c++) {
    pinMode(cols_[c], INPUT_PULLUP);
  }

  scanRow_ = 0;
  waitingRelease_ = false;
  lastKey_ = 0;
  lastKeyMs_ = 0;
}

void KeypadDriver::setAllRowsHigh_() {
  for (uint8_t r = 0; r < nRows_; r++) digitalWrite(rows_[r], HIGH);
}

void KeypadDriver::driveRow_(uint8_t r) {
  setAllRowsHigh_();
  digitalWrite(rows_[r], LOW);
}

int KeypadDriver::readColPressed_() const {
  for (uint8_t c = 0; c < nCols_; c++) {
    if (digitalRead(cols_[c]) == LOW) return (int)c;
  }
  return -1;
}

char KeypadDriver::mapKey_(uint8_t r, uint8_t c) const {
  return keymap_[r * nCols_ + c];
}

char KeypadDriver::update(uint32_t nowMs) {
  // ถ้ายังไม่ปล่อยปุ่ม จะไม่ยิงซ้ำ
  if (waitingRelease_) {
    // scan ทุกแถวเพื่อเช็คว่าปล่อยหมดแล้ว
    bool anyDown = false;
    for (uint8_t r = 0; r < nRows_; r++) {
      driveRow_(r);
      if (readColPressed_() >= 0) { anyDown = true; break; }
    }
    setAllRowsHigh_();
    if (!anyDown) waitingRelease_ = false;
    return 0;
  }

  // scan ทีละแถว (non-blocking)
  driveRow_(scanRow_);
  int col = readColPressed_();
  setAllRowsHigh_();

  if (col >= 0) {
    char k = mapKey_(scanRow_, (uint8_t)col);

    // debounce + กันกดค้าง
    if ((nowMs - lastKeyMs_) >= debounce_ms_ || k != lastKey_) {
      lastKey_ = k;
      lastKeyMs_ = nowMs;
      waitingRelease_ = true;
      return k;
    }
  }

  scanRow_++;
  if (scanRow_ >= nRows_) scanRow_ = 0;
  return 0;
}
