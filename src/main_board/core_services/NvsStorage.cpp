#include "core_services/NvsStorage.h"

#include <Preferences.h>

namespace {

Preferences gPreferences;
constexpr const char* kNamespace = "sec-home";
constexpr const char* kLegacyModeKey = "mode";
constexpr const char* kLatestModeKey = "latest_mode";
constexpr const char* kIsNightKey = "is_night";

uint8_t baseModeToRaw(ConfigurationSharedTypes::Mode mode) {
  switch (mode) {
    case ConfigurationSharedTypes::Mode::disarm: return 1;
    case ConfigurationSharedTypes::Mode::away: return 2;
    case ConfigurationSharedTypes::Mode::night: return 1;
    default: return 1; // Fallback to safe default (disarm).
  }
}

ConfigurationSharedTypes::Mode rawToBaseMode(uint8_t raw) {
  switch (raw) {
    case 1: return ConfigurationSharedTypes::Mode::disarm;
    case 2: return ConfigurationSharedTypes::Mode::away;
    default: return ConfigurationSharedTypes::Mode::disarm;
  }
}

} // namespace

namespace CoreServices {

void NvsStorage::begin() {
  if (started_) return;
  started_ = gPreferences.begin(kNamespace, false);
}

bool NvsStorage::loadModeState(ConfigurationSharedTypes::Mode& outLatestMode, bool& outIsNight) {
  begin();
  if (!started_) return false;

  const uint8_t rawLatest = gPreferences.isKey(kLatestModeKey)
    ? gPreferences.getUChar(kLatestModeKey, baseModeToRaw(ConfigurationSharedTypes::Mode::disarm))
    : gPreferences.getUChar(kLegacyModeKey, baseModeToRaw(ConfigurationSharedTypes::Mode::disarm));

  outLatestMode = rawToBaseMode(rawLatest);
  outIsNight = gPreferences.getBool(kIsNightKey, false);
  return true;
}

void NvsStorage::saveModeState(ConfigurationSharedTypes::Mode latestMode, bool isNight) {
  begin();
  if (!started_) return;

  gPreferences.putUChar(kLatestModeKey, baseModeToRaw(latestMode));
  gPreferences.putBool(kIsNightKey, isNight);
}

} // namespace CoreServices
