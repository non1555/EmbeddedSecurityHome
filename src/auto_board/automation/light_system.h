#pragma once

namespace LightSystem {

void init();
bool nextLightState(
  bool autoEnabled,
  bool currentLightOn,
  bool luxOk,
  float lux,
  bool allowByMainMode,
  bool allowByMainPresence
);

} // namespace LightSystem
