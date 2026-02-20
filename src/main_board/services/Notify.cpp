#include "Notify.h"

#ifndef SERIAL_NOTIFY_ENABLED_DEFAULT
#define SERIAL_NOTIFY_ENABLED_DEFAULT 0
#endif

void Notify::begin() {
  serialEnabled_ = (SERIAL_NOTIFY_ENABLED_DEFAULT != 0);
}

void Notify::update(uint32_t) {}

void Notify::setSerialEnabled(bool enabled) {
  serialEnabled_ = enabled;
}

void Notify::send(const String& msg) {
  if (!serialEnabled_) return;
  Serial.print("[NOTIFY] ");
  Serial.println(msg);
}
