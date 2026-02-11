#include "KeypadInput.h"

KeypadInput::KeypadInput(uint8_t id)
: id_(id), len_(0), hasEvent_(false), pending_{EventType::disarm, 0, 0}, lastKeyMs_(0) {
  strncpy(armCode_, "1234", 5);
  strncpy(disarmCode_, "0000", 5);
  clear_();
}

void KeypadInput::begin() {
  clear_();
  hasEvent_ = false;
  lastKeyMs_ = 0;
}

void KeypadInput::setArmCode(const char* code4) {
  strncpy(armCode_, code4, 4);
  armCode_[4] = '\0';
}

void KeypadInput::setDisarmCode(const char* code4) {
  strncpy(disarmCode_, code4, 4);
  disarmCode_[4] = '\0';
}

void KeypadInput::clear_() {
  len_ = 0;
  buf_[0] = '\0';
}

bool KeypadInput::match_(const char* a, const char* b) const {
  return strncmp(a, b, 4) == 0;
}

void KeypadInput::feedKey(char k, uint32_t nowMs) {
  // กัน key เด้งถี่เกิน (เผื่อสาย/คอนแทค)
  if ((nowMs - lastKeyMs_) < 30) return;
  lastKeyMs_ = nowMs;

  if (k == '*') { clear_(); return; }

  if (k >= '0' && k <= '9') {
    if (len_ < 4) {
      buf_[len_++] = k;
      buf_[len_] = '\0';
    }
    return;
  }

  if (k == '#') {
    if (len_ == 4) {
      if (match_(buf_, armCode_)) {
        pending_ = {EventType::arm_night, nowMs, id_};
        hasEvent_ = true;
      } else if (match_(buf_, disarmCode_)) {
        pending_ = {EventType::disarm, nowMs, id_};
        hasEvent_ = true;
      }
    }
    clear_();
    return;
  }
}

bool KeypadInput::poll(uint32_t, Event& out) {
  if (!hasEvent_) return false;
  hasEvent_ = false;
  out = pending_;
  return true;
}
