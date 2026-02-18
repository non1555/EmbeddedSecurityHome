#pragma once

#include <Arduino.h>

// Lightweight replay guard for short-lived nonces.
class ReplayGuard {
public:
  bool accept(const String& nonce, uint32_t nowMs, uint32_t ttlMs) {
    if (nonce.length() == 0 || ttlMs == 0) return false;

    const uint32_t h = fnv1a_(nonce);
    for (size_t i = 0; i < kSlots; ++i) {
      if (!slots_[i].used) continue;
      if (expired_(nowMs, slots_[i].expiresAtMs)) continue;
      if (slots_[i].hash == h) return false;
    }

    slots_[cursor_].used = true;
    slots_[cursor_].hash = h;
    slots_[cursor_].expiresAtMs = nowMs + ttlMs;
    cursor_ = (cursor_ + 1u) % kSlots;
    return true;
  }

private:
  static constexpr size_t kSlots = 24;

  struct Slot {
    bool used = false;
    uint32_t hash = 0;
    uint32_t expiresAtMs = 0;
  };

  Slot slots_[kSlots]{};
  size_t cursor_ = 0;

  static bool expired_(uint32_t nowMs, uint32_t expiresAtMs) {
    return (int32_t)(nowMs - expiresAtMs) >= 0;
  }

  static uint32_t fnv1a_(const String& s) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < s.length(); ++i) {
      h ^= static_cast<uint8_t>(s[i]);
      h *= 16777619u;
    }
    return h;
  }
};
