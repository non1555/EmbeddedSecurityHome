#include "temp_system.h"

#include <Arduino.h>
#include "hardware/AutoHardwareConfig.h"

namespace TempSystem {

void init() {}

bool nextFanState(
  bool autoEnabled,
  bool currentFanOn,
  float tempC,
  bool allowByMainMode,
  bool allowByMainPresence
) {
  if (!autoEnabled) return currentFanOn;
  if (!allowByMainMode || !allowByMainPresence) return false;
  if (isnan(tempC)) return currentFanOn;

  if (!currentFanOn && tempC >= AutoHw::FAN_ON_C) return true;
  if (currentFanOn && tempC <= AutoHw::FAN_OFF_C) return false;
  return currentFanOn;
}

} // namespace TempSystem
