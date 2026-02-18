#include <Arduino.h>

#include "hardware/AutoHardwareConfig.h"

namespace LightSystem {

void init() {}

bool nextLightState(
  bool autoEnabled,
  bool currentLightOn,
  bool luxOk,
  float lux,
  bool allowByMainMode,
  bool allowByMainPresence
) {
  if (!autoEnabled) return currentLightOn;
  if (!allowByMainMode || !allowByMainPresence) return false;
  if (!luxOk || isnan(lux)) return currentLightOn;

  if (!currentLightOn && lux < AutoHw::LUX_ON) return true;
  if (currentLightOn && lux > AutoHw::LUX_OFF) return false;
  return currentLightOn;
}

} // namespace LightSystem
