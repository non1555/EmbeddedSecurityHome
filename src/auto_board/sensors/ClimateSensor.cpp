#include "sensors/ClimateSensor.h"

#include <Arduino.h>
#include <DHT.h>

#include "hardware/AutoHardwareConfig.h"

namespace {

#define DHTTYPE DHT11

DHT dht(AutoHw::PIN_DHT, DHTTYPE);

} // namespace

namespace ClimateSensor {

void begin() {
  if (AutoHw::PIN_DHT == AutoHw::PIN_UNUSED) return;
  dht.begin();
}

bool available() {
  return AutoHw::PIN_DHT != AutoHw::PIN_UNUSED;
}

void read(float& tempCOut, float& humOut) {
  if (!available()) {
    tempCOut = NAN;
    humOut = NAN;
    return;
  }

  tempCOut = dht.readTemperature();
  humOut = dht.readHumidity();
}

} // namespace ClimateSensor
