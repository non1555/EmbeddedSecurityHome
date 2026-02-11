#pragma once
#include <Arduino.h>
#include <Wire.h>

class I2CKeypadDriver {
public:
  // PCF8574 mapping:
  // P0..P3 -> rows, P4..P7 -> cols
  I2CKeypadDriver(TwoWire* wire, uint8_t addr7,
                  const char* keymap, uint32_t debounce_ms = 60);

  bool begin();
  char update(uint32_t nowMs);

private:
  TwoWire* wire_;
  uint8_t addr7_;
  const char* keymap_;
  uint32_t debounce_ms_;

  uint8_t scanRow_;
  bool waitingRelease_;
  char lastKey_;
  uint32_t lastKeyMs_;
  uint8_t shadow_;

  bool writePort_(uint8_t value);
  int readColPressed_();
  bool setRowActive_(uint8_t r);
  bool setAllRowsHigh_();
  char mapKey_(uint8_t r, uint8_t c) const;
};
