#include "hardware_abstraction_layer/KeypadController.h"

#include <cstring>

#ifndef DOOR_CODE
#define DOOR_CODE ""
#endif

namespace {
constexpr uint8_t kRowMask = 0x0F;
constexpr uint8_t kDebounceMs = 60;
} // namespace

namespace HardwareAbstractionLayer {

void KeypadController::begin() {
  scanRow_ = 0;
  waitingRelease_ = false;
  lastKey_ = 0;
  lastKeyMs_ = 0;
  ioShadow_ = 0xFF;
  setAllRowsHigh_();

  setDoorCodeFromBuild_();
  clear_();
  submitReady_ = false;
  submitOk_ = false;
}

bool KeypadController::poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  const char key = scanKey_(nowMs);
  if (!key) return false;

  if (key == 'A') {
    out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::door_hold_warn_silence, nowMs, 0};
    return true;
  }

  if (key == 'B') {
    out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::keypad_help_request, nowMs, 0};
    return true;
  }

  if (key == 'C') {
    clear_();
    return false;
  }

  if (key == '*') {
    if (inputLength_ > 0) {
      --inputLength_;
      inputBuffer_[inputLength_] = '\0';
    }
    return false;
  }

  if (key >= '0' && key <= '9') {
    if (inputLength_ < 4) {
      inputBuffer_[inputLength_++] = key;
      inputBuffer_[inputLength_] = '\0';
    }
    return false;
  }

  if (key == '#') {
    submitReady_ = true;
    submitOk_ = (inputLength_ == 4) && matchesDoorCode_();
    out = ConfigurationSharedTypes::Event{
      submitOk_ ? ConfigurationSharedTypes::EventType::door_code_unlock
                : ConfigurationSharedTypes::EventType::door_code_bad,
      nowMs,
      0};
    clear_();
    return true;
  }

  return false;
}

const char* KeypadController::buffer() const {
  return inputBuffer_;
}

uint8_t KeypadController::length() const {
  return inputLength_;
}

bool KeypadController::consumeSubmitResult(bool& ok) {
  if (!submitReady_) return false;
  submitReady_ = false;
  ok = submitOk_;
  return true;
}

void KeypadController::clear_() {
  inputLength_ = 0;
  inputBuffer_[0] = '\0';
}

void KeypadController::setDoorCodeFromBuild_() {
  const char* raw = DOOR_CODE;
  if (!isValidCode_(raw)) {
    raw = "1234";
  }
  std::strncpy(doorCode_, raw, sizeof(doorCode_) - 1);
  doorCode_[sizeof(doorCode_) - 1] = '\0';
}

bool KeypadController::isValidCode_(const char* code) const {
  if (!code || std::strlen(code) != 4) return false;
  for (size_t i = 0; i < 4; ++i) {
    if (code[i] < '0' || code[i] > '9') return false;
  }
  return true;
}

bool KeypadController::matchesDoorCode_() const {
  return std::strncmp(inputBuffer_, doorCode_, 4) == 0;
}

bool KeypadController::writePort_(uint8_t value) {
  Wire.beginTransmission(ConfigurationSharedTypes::Config::KEYPAD_I2C_ADDR);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

int KeypadController::readPressedColumn_() {
  const int count = Wire.requestFrom((int)ConfigurationSharedTypes::Config::KEYPAD_I2C_ADDR, 1);
  if (count != 1) return -1;

  const uint8_t value = (uint8_t)Wire.read();
  for (uint8_t col = 0; col < 4; ++col) {
    const uint8_t bit = (uint8_t)(1u << (4u + col));
    if ((value & bit) == 0) return (int)col;
  }

  return -1;
}

bool KeypadController::setRowActive_(uint8_t row) {
  if (row >= 4) return false;
  ioShadow_ = (uint8_t)((ioShadow_ & (uint8_t)~kRowMask) | kRowMask);
  ioShadow_ = (uint8_t)(ioShadow_ & (uint8_t)~(1u << row));
  return writePort_(ioShadow_);
}

bool KeypadController::setAllRowsHigh_() {
  ioShadow_ = (uint8_t)((ioShadow_ & (uint8_t)~kRowMask) | kRowMask);
  return writePort_(ioShadow_);
}

char KeypadController::mapKey_(uint8_t row, uint8_t col) const {
  return ConfigurationSharedTypes::Config::KEYPAD_MAP[(row * 4u) + col];
}

char KeypadController::scanKey_(uint32_t nowMs) {
  if (waitingRelease_) {
    bool anyPressed = false;

    for (uint8_t row = 0; row < 4; ++row) {
      if (!setRowActive_(row)) return 0;
      if (readPressedColumn_() >= 0) {
        anyPressed = true;
        break;
      }
    }

    setAllRowsHigh_();
    if (!anyPressed) {
      waitingRelease_ = false;
    }

    return 0;
  }

  if (!setRowActive_(scanRow_)) return 0;
  const int column = readPressedColumn_();
  setAllRowsHigh_();

  if (column >= 0) {
    const char key = mapKey_(scanRow_, (uint8_t)column);
    if ((nowMs - lastKeyMs_) >= kDebounceMs || key != lastKey_) {
      lastKey_ = key;
      lastKeyMs_ = nowMs;
      waitingRelease_ = true;
      return key;
    }
  }

  scanRow_ = (uint8_t)((scanRow_ + 1u) & 0x03u);
  return 0;
}

} // namespace HardwareAbstractionLayer
