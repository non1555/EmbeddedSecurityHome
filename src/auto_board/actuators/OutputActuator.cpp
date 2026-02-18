#include "actuators/OutputActuator.h"

#include <Arduino.h>

#include "hardware/AutoHardwareConfig.h"

namespace OutputActuator {

void init() {
  if (AutoHw::PIN_LIGHT_LED != AutoHw::PIN_UNUSED) {
    pinMode(AutoHw::PIN_LIGHT_LED, OUTPUT);
    digitalWrite(AutoHw::PIN_LIGHT_LED, LOW);
  }

  const bool fanReady =
    (AutoHw::PIN_L293D_IN1 != AutoHw::PIN_UNUSED) &&
    (AutoHw::PIN_L293D_IN2 != AutoHw::PIN_UNUSED);
  if (!fanReady) return;

  pinMode(AutoHw::PIN_L293D_IN1, OUTPUT);
  pinMode(AutoHw::PIN_L293D_IN2, OUTPUT);
  if (AutoHw::PIN_L293D_EN != AutoHw::PIN_UNUSED) {
    pinMode(AutoHw::PIN_L293D_EN, OUTPUT);
    digitalWrite(AutoHw::PIN_L293D_EN, HIGH);
  } else {
    Serial.println("[auto] FAN EN pin not configured; tie L293D EN HIGH in hardware");
  }

  digitalWrite(AutoHw::PIN_L293D_IN1, LOW);
  digitalWrite(AutoHw::PIN_L293D_IN2, LOW);
}

void apply(const State& state) {
  if (AutoHw::PIN_LIGHT_LED != AutoHw::PIN_UNUSED) {
    digitalWrite(AutoHw::PIN_LIGHT_LED, state.lightOn ? HIGH : LOW);
  }

  const bool fanReady =
    (AutoHw::PIN_L293D_IN1 != AutoHw::PIN_UNUSED) &&
    (AutoHw::PIN_L293D_IN2 != AutoHw::PIN_UNUSED);
  if (!fanReady) return;

  if (state.fanOn) {
    digitalWrite(AutoHw::PIN_L293D_IN1, HIGH);
    digitalWrite(AutoHw::PIN_L293D_IN2, LOW);
  } else {
    digitalWrite(AutoHw::PIN_L293D_IN1, LOW);
    digitalWrite(AutoHw::PIN_L293D_IN2, LOW);
  }
}

} // namespace OutputActuator
