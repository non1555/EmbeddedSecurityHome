#include "I2CKeypadDriver.h"

namespace {
constexpr uint8_t ROW_MASK = 0x0F; // P0..P3
constexpr uint8_t COL_MASK = 0xF0; // P4..P7
}

I2CKeypadDriver::I2CKeypadDriver(TwoWire* wire, uint8_t addr7,
                                 const char* keymap, uint32_t debounce_ms)
: wire_(wire),
  addr7_(addr7),
  keymap_(keymap),
  debounce_ms_(debounce_ms),
  scanRow_(0),
  waitingRelease_(false),
  lastKey_(0),
  lastKeyMs_(0),
  shadow_(0xFF) {}

bool I2CKeypadDriver::writePort_(uint8_t value) {
  if (!wire_) return false;
  wire_->beginTransmission(addr7_);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

int I2CKeypadDriver::readColPressed_() {
  if (!wire_) return -1;
  int n = wire_->requestFrom((int)addr7_, 1);
  if (n != 1) return -1;
  uint8_t v = (uint8_t)wire_->read();

  for (uint8_t c = 0; c < 4; c++) {
    uint8_t bit = (uint8_t)(1u << (4u + c));
    if ((v & bit) == 0) return (int)c;
  }
  return -1;
}

bool I2CKeypadDriver::setAllRowsHigh_() {
  shadow_ = (uint8_t)((shadow_ & (uint8_t)~ROW_MASK) | ROW_MASK);
  return writePort_(shadow_);
}

bool I2CKeypadDriver::setRowActive_(uint8_t r) {
  if (r >= 4) return false;
  shadow_ = (uint8_t)((shadow_ & (uint8_t)~ROW_MASK) | ROW_MASK);
  shadow_ = (uint8_t)(shadow_ & (uint8_t)~(1u << r));
  return writePort_(shadow_);
}

char I2CKeypadDriver::mapKey_(uint8_t r, uint8_t c) const {
  return keymap_[r * 4u + c];
}

bool I2CKeypadDriver::begin() {
  scanRow_ = 0;
  waitingRelease_ = false;
  lastKey_ = 0;
  lastKeyMs_ = 0;
  shadow_ = 0xFF;
  return setAllRowsHigh_();
}

char I2CKeypadDriver::update(uint32_t nowMs) {
  if (waitingRelease_) {
    bool anyDown = false;
    for (uint8_t r = 0; r < 4; r++) {
      if (!setRowActive_(r)) return 0;
      if (readColPressed_() >= 0) {
        anyDown = true;
        break;
      }
    }
    setAllRowsHigh_();
    if (!anyDown) waitingRelease_ = false;
    return 0;
  }

  if (!setRowActive_(scanRow_)) return 0;
  int col = readColPressed_();
  setAllRowsHigh_();

  if (col >= 0) {
    char k = mapKey_(scanRow_, (uint8_t)col);
    if ((nowMs - lastKeyMs_) >= debounce_ms_ || k != lastKey_) {
      lastKey_ = k;
      lastKeyMs_ = nowMs;
      waitingRelease_ = true;
      return k;
    }
  }

  scanRow_ = (uint8_t)((scanRow_ + 1u) & 0x03u);
  return 0;
}
