#include "pipelines/AutomationPipeline.h"

#include "automation/light_system.h"
#include "automation/temp_system.h"

namespace AutomationPipeline {

bool nextLight(
  bool autoEnabled,
  bool currentLightOn,
  bool luxOk,
  float lux,
  bool allowByMainMode,
  bool allowByMainPresence
) {
  return LightSystem::nextLightState(
    autoEnabled,
    currentLightOn,
    luxOk,
    lux,
    allowByMainMode,
    allowByMainPresence
  );
}

bool nextFan(bool autoEnabled, bool currentFanOn, float tempC) {
  return TempSystem::nextFanState(autoEnabled, currentFanOn, tempC);
}

} // namespace AutomationPipeline
