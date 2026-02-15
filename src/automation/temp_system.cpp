#include "temp_system.h"

#include <Arduino.h>
#include <DHT.h>

#include "app/HardwareConfig.h"

#define DHTTYPE DHT11

namespace {
DHT dht(HwCfg::PIN_DHT, DHTTYPE);
bool fanState = false;
uint32_t nextPollMs = 0;

static void fanWrite(bool on) {
  if (HwCfg::PIN_FAN == HwCfg::PIN_UNUSED) return;
  ledcWrite(HwCfg::FAN_LEDC_CH, on ? HwCfg::FAN_PWM_DUTY_ON : 0);
}
} // namespace

void initTempSystem() {
  if (HwCfg::PIN_DHT != HwCfg::PIN_UNUSED) {
    dht.begin();
  }

  if (HwCfg::PIN_FAN != HwCfg::PIN_UNUSED) {
    ledcSetup(HwCfg::FAN_LEDC_CH, HwCfg::FAN_PWM_HZ, HwCfg::FAN_PWM_RES_BITS);
    ledcAttachPin(HwCfg::PIN_FAN, HwCfg::FAN_LEDC_CH);
    fanWrite(false);
  }

  fanState = false;
  nextPollMs = 0;
}

void updateTempSystem() {
  if (HwCfg::PIN_DHT == HwCfg::PIN_UNUSED) return;

  const uint32_t nowMs = millis();
  if (nextPollMs != 0 && nowMs < nextPollMs) return;
  nextPollMs = nowMs + HwCfg::TEMP_POLL_MS;

  const float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("DHT error");
    return;
  }

  Serial.print("Temp: ");
  Serial.println(temp);

  if (!fanState && temp >= HwCfg::TEMP_ON_C) {
    fanState = true;
    fanWrite(true);
  } else if (fanState && temp <= HwCfg::TEMP_OFF_C) {
    fanState = false;
    fanWrite(false);
  }
}
