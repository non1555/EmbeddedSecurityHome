#include "Notify.h"

void Notify::begin() {}

void Notify::update(uint32_t) {}

void Notify::send(const String& msg) {
  Serial.print("[NOTIFY] ");
  Serial.println(msg);
}
