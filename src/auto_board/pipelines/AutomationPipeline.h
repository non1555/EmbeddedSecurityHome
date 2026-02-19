#pragma once

namespace AutomationPipeline {

bool nextLight(
  bool autoEnabled,
  bool currentLightOn,
  bool luxOk,
  float lux,
  bool allowByMainMode,
  bool allowByMainPresence
);

bool nextFan(
  bool autoEnabled,
  bool currentFanOn,
  float tempC,
  bool allowByMainMode,
  bool allowByMainPresence
);

} // namespace AutomationPipeline
