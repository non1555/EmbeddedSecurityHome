#include <Arduino.h>
#include <Wire.h>

#include "fw_auto/AutoHardwareConfig.h"

namespace {

// BH1750 default 7-bit address is often 0x23 (ADDR pin LOW) or 0x5C (ADDR pin HIGH).
constexpr uint8_t BH1750_ADDR = 0x23;
constexpr uint8_t CMD_POWER_ON = 0x01;
constexpr uint8_t CMD_RESET = 0x07;
constexpr uint8_t CMD_CONT_HIRES = 0x10;

// Hysteresis thresholds (lux): turn ON when dark, turn OFF when bright.
constexpr float LUX_ON = 120.0f;
constexpr float LUX_OFF = 180.0f;

constexpr uint32_t SAMPLE_MS = 400;
uint32_t nextSampleMs = 0;

bool bhReady = false;
bool relayOn = false;

bool bhWrite(uint8_t cmd) {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

bool bhInit() {
  if (!bhWrite(CMD_POWER_ON)) return false;
  delay(10);
  if (!bhWrite(CMD_RESET)) return false;
  delay(10);
  if (!bhWrite(CMD_CONT_HIRES)) return false;
  delay(180); // first conversion time
  return true;
}

bool bhReadLux(float& luxOut) {
  Wire.requestFrom((int)BH1750_ADDR, 2);
  if (Wire.available() < 2) return false;
  const uint16_t raw = (uint16_t(Wire.read()) << 8) | uint16_t(Wire.read());
  luxOut = raw / 1.2f;
  return true;
}

void applyRelay(bool on) {
  relayOn = on;
  if (AutoHw::PIN_RELAY_LIGHT == AutoHw::PIN_UNUSED) return;
  digitalWrite(AutoHw::PIN_RELAY_LIGHT, on ? HIGH : LOW);
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println();
  Serial.println("=== Automation Light Local (BH1750 -> Relay) ===");
  Serial.print("I2C SDA/SCL: ");
  Serial.print(AutoHw::PIN_I2C_SDA);
  Serial.print("/");
  Serial.println(AutoHw::PIN_I2C_SCL);
  Serial.print("Relay pin: ");
  Serial.println(AutoHw::PIN_RELAY_LIGHT);
  Serial.print("BH1750 addr: 0x");
  Serial.println(BH1750_ADDR, HEX);
  Serial.print("LUX_ON/LUX_OFF: ");
  Serial.print(LUX_ON, 1);
  Serial.print("/");
  Serial.println(LUX_OFF, 1);

  if (AutoHw::PIN_RELAY_LIGHT != AutoHw::PIN_UNUSED) {
    pinMode(AutoHw::PIN_RELAY_LIGHT, OUTPUT);
    applyRelay(false);
  } else {
    Serial.println("WARNING: PIN_RELAY_LIGHT is PIN_UNUSED.");
  }

  Wire.begin(AutoHw::PIN_I2C_SDA, AutoHw::PIN_I2C_SCL);
  bhReady = bhInit();
  Serial.println(bhReady ? "BH1750 init OK" : "BH1750 init FAILED");
}

void loop() {
  const uint32_t now = millis();
  if (nextSampleMs != 0 && now < nextSampleMs) return;
  nextSampleMs = now + SAMPLE_MS;

  float lux = NAN;
  bool ok = false;
  if (bhReady) ok = bhReadLux(lux);

  if (ok) {
    if (!relayOn && lux < LUX_ON) {
      applyRelay(true);
    } else if (relayOn && lux > LUX_OFF) {
      applyRelay(false);
    }
  }

  Serial.print("lux=");
  if (ok) Serial.print(lux, 1);
  else Serial.print("ERR");
  Serial.print(" relay=");
  Serial.println(relayOn ? "ON" : "OFF");
}

