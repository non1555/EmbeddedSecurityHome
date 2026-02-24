#pragma once

#include "configuration_shared_types/Types.h"

namespace CoreServices {

class NvsStorage {
public:
  void begin(); // Opens NVS namespace (Preferences) once. Params: none.
  bool loadModeState(ConfigurationSharedTypes::Mode& outLatestMode,
                     bool& outIsNight); // Loads persisted base mode and night override flag from NVS. Params: outLatestMode=output base mode (disarm/away), outIsNight=output override flag.
  void saveModeState(ConfigurationSharedTypes::Mode latestMode,
                     bool isNight); // Persists base mode and night override flag to NVS. Params: latestMode=base mode (disarm/away), isNight=night override flag.

private:
  bool started_ = false;
};

} // namespace CoreServices
