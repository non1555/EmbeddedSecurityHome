#pragma once
#include <Arduino.h>
#include "app/Events.h"

class KeypadInput {
public:
  KeypadInput(uint8_t id);

  void begin();

  void setDoorCode(const char* code4);

  // feed a raw key from keypad driver
  void feedKey(char k, uint32_t nowMs);

  // returns true if an Event is produced
  bool poll(uint32_t nowMs, Event& out);

  const char* buf() const { return buf_; }
  uint8_t len() const { return len_; }

  enum class SubmitResult : uint8_t { none, ok, bad };
  bool takeSubmitResult(SubmitResult& out);

private:
  uint8_t id_;

  char doorCode_[5];

  char buf_[5];
  uint8_t len_;

  bool hasEvent_;
  Event pending_;

  uint32_t lastKeyMs_;
  SubmitResult lastSubmit_;

  void clear_();
  bool match_(const char* a, const char* b) const;
};
