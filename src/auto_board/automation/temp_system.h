#pragma once

namespace TempSystem {

void init();
bool nextFanState(
  bool autoEnabled,
  bool currentFanOn,
  float tempC,
  bool allowByMainMode,
  bool allowByMainPresence
);

} // namespace TempSystem
