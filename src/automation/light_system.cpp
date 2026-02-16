#include <Arduino.h>

#include "app/HardwareConfig.h"
#include "automation/presence.h"

void initLightSystem() {
  if (HwCfg::PIN_RELAY_LIGHT == HwCfg::PIN_UNUSED) return;
  pinMode(HwCfg::PIN_RELAY_LIGHT, OUTPUT);
  digitalWrite(HwCfg::PIN_RELAY_LIGHT, LOW);
}

void updateLightSystem() {
  if (HwCfg::LIGHT_SENSOR_ADC_PIN == HwCfg::PIN_UNUSED) return;
  if (HwCfg::PIN_RELAY_LIGHT == HwCfg::PIN_UNUSED) return;

  static uint32_t nextMs = 0;
  const uint32_t nowMs = millis();
  if (nextMs != 0 && nowMs < nextMs) return;
  nextMs = nowMs + 400;

  const int lightValue = analogRead(HwCfg::LIGHT_SENSOR_ADC_PIN);

  const bool shouldOn = isSomeoneHome && (lightValue > HwCfg::LIGHT_ADC_THRESHOLD);
  digitalWrite(HwCfg::PIN_RELAY_LIGHT, shouldOn ? HIGH : LOW);

  Serial.print("Light: ");
  Serial.print(lightValue);
  Serial.print(" relay=");
  Serial.println(shouldOn ? "ON" : "OFF");
}
