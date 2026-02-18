#pragma once

#include <stdint.h>

namespace LightSensor {

void begin();
bool isReady();
uint8_t address();
bool readLux(float& luxOut);

} // namespace LightSensor
