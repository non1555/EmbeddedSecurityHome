#include "KeypadInput.h"

KeypadInput::KeypadInput(uint8_t id)
: id_(id),
  len_(0),
  hasEvent_(false),
  pending_{EventType::disarm, 0, 0},
  lastKeyMs_(0),
  lastSubmit_(SubmitResult::none) {
  // Safe default until configured from env: keypad unlock disabled.
  strncpy(doorCode_, "ABCD", 5);
  clear_();
}

void KeypadInput::begin() {
  clear_();
  hasEvent_ = false;
  lastKeyMs_ = 0;
  lastSubmit_ = SubmitResult::none;
}

void KeypadInput::setDoorCode(const char* code4) {
  strncpy(doorCode_, code4, 4);
  doorCode_[4] = '\0';
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

  if (k == '*' || k == 'D') {
    // backspace
    if (len_ > 0) {
      len_--;
      buf_[len_] = '\0';
    }
    return;
  }

  if (k == 'C') { clear_(); return; }

  if (k >= '0' && k <= '9') {
    if (len_ < 4) {
      buf_[len_++] = k;
      buf_[len_] = '\0';
    }
    return;
  }

  if (k == '#') {
    if (len_ == 4 && match_(buf_, doorCode_)) {
      pending_ = {EventType::door_code_unlock, nowMs, id_};
      hasEvent_ = true;
      lastSubmit_ = SubmitResult::ok;
    } else {
      lastSubmit_ = SubmitResult::bad;
      pending_ = {EventType::door_code_bad, nowMs, id_};
      hasEvent_ = true;
    }
    clear_();
    return;
  }
}

bool KeypadInput::takeSubmitResult(SubmitResult& out) {
  if (lastSubmit_ == SubmitResult::none) return false;
  out = lastSubmit_;
  lastSubmit_ = SubmitResult::none;
  return true;
}

bool KeypadInput::poll(uint32_t, Event& out) {
  if (!hasEvent_) return false;
  hasEvent_ = false;
  out = pending_;
  return true;
}
