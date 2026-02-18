#pragma once
#include <Arduino.h>

namespace AutoRuntime {

void begin();
void tick(uint32_t nowMs);

} // namespace AutoRuntime
