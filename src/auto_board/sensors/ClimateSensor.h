#pragma once

namespace ClimateSensor {

void begin();
bool available();
void read(float& tempCOut, float& humOut);

} // namespace ClimateSensor
