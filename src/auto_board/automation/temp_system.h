#pragma once

namespace TempSystem {

void init();
bool nextFanState(bool autoEnabled, bool currentFanOn, float tempC);

} // namespace TempSystem
