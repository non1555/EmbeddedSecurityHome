#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/Types.h"

namespace HardwareAbstractionLayer {

class KeypadController {
public:
  void begin(); // Initializes keypad scanner state and configured door code. Params: none.
  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls keypad and emits semantic keypad events. Params: nowMs=current timestamp in ms, out=event output.
  bool consumeInputActivity(); // Returns whether a non-event keypad edit happened this tick and clears the flag. Params: none.

  const char* buffer() const; // Returns pointer to current entered PIN buffer. Params: none.
  uint8_t length() const; // Returns current entered PIN length. Params: none.
  bool consumeSubmitResult(bool& ok); // Returns latest submit result once and clears flag. Params: ok=output submit pass/fail flag.

private:
  char doorCode_[5] = {0, 0, 0, 0, 0};
  char inputBuffer_[5] = {0, 0, 0, 0, 0};
  uint8_t inputLength_ = 0;

  bool submitReady_ = false;
  bool submitOk_ = false;
  bool inputEdited_ = false;

  uint8_t scanRow_ = 0;
  bool waitingRelease_ = false;
  char lastKey_ = 0;
  uint32_t lastKeyMs_ = 0;
  uint8_t ioShadow_ = 0xFF;

  void clear_(); // Clears entered PIN buffer. Params: none.
  void setDoorCodeFromBuild_(); // Loads build-time DOOR_CODE or fallback default. Params: none.
  bool isValidCode_(const char* code) const; // Validates that a code is exactly 4 digits. Params: code=c-string code to validate.
  bool matchesDoorCode_() const; // Compares entered PIN with configured door code. Params: none.

  bool writePort_(uint8_t value); // Writes raw byte to keypad I2C expander. Params: value=port bitmask.
  int readPressedColumn_(); // Reads active column index from keypad matrix or -1. Params: none.
  bool setRowActive_(uint8_t row); // Drives one row low for scan step. Params: row=row index (0..3).
  bool setAllRowsHigh_(); // Releases all rows high (idle state). Params: none.
  char mapKey_(uint8_t row, uint8_t col) const; // Maps row/column to key symbol. Params: row=row index (0..3), col=column index (0..3).
  char scanKey_(uint32_t nowMs); // Runs one scan cycle with debounce and release gating. Params: nowMs=current timestamp in ms.
};

} // namespace HardwareAbstractionLayer
