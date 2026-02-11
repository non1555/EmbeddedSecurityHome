#pragma once
#include <Arduino.h>

class KeypadDriver {
public:
  // rows: OUTPUT, cols: INPUT_PULLUP
  KeypadDriver(const uint8_t* rows, uint8_t nRows,
               const uint8_t* cols, uint8_t nCols,
               const char* keymap,            // length = nRows*nCols, row-major
               uint32_t debounce_ms = 60);

  void begin();

  // call frequently, returns 0 if no new key pressed
  char update(uint32_t nowMs);

private:
  const uint8_t* rows_;
  const uint8_t* cols_;
  uint8_t nRows_;
  uint8_t nCols_;
  const char* keymap_;
  uint32_t debounce_ms_;

  uint8_t scanRow_;
  bool waitingRelease_;
  char lastKey_;
  uint32_t lastKeyMs_;

  void setAllRowsHigh_();
  void driveRow_(uint8_t r);
  int readColPressed_() const;      // returns col index or -1
  char mapKey_(uint8_t r, uint8_t c) const;
};
