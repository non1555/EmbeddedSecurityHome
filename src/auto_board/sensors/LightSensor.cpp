#include "sensors/LightSensor.h"

#include <Arduino.h>
#include <Wire.h>

#include "hardware/AutoHardwareConfig.h"

namespace {

uint8_t addr = 0;
bool ready = false;

constexpr uint8_t BH_CMD_POWER_ON = 0x01;
constexpr uint8_t BH_CMD_RESET = 0x07;
constexpr uint8_t BH_CMD_CONT_HIRES = 0x10;

bool bhWrite(uint8_t i2cAddr, uint8_t cmd) {
  Wire.beginTransmission(i2cAddr);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

bool initAt(uint8_t i2cAddr) {
  if (!bhWrite(i2cAddr, BH_CMD_POWER_ON)) return false;
  delay(10);
  if (!bhWrite(i2cAddr, BH_CMD_RESET)) return false;
  delay(10);
  if (!bhWrite(i2cAddr, BH_CMD_CONT_HIRES)) return false;
  delay(180);
  return true;
}

} // namespace

namespace LightSensor {

void begin() {
  Wire.begin(AutoHw::PIN_I2C_SDA, AutoHw::PIN_I2C_SCL);

  if (initAt(AutoHw::BH1750_ADDR_PRIMARY)) {
    addr = AutoHw::BH1750_ADDR_PRIMARY;
    ready = true;
    return;
  }

  if (initAt(AutoHw::BH1750_ADDR_SECONDARY)) {
    addr = AutoHw::BH1750_ADDR_SECONDARY;
    ready = true;
    return;
  }

  addr = 0;
  ready = false;
}

bool isReady() {
  return ready;
}

uint8_t address() {
  return addr;
}

bool readLux(float& luxOut) {
  if (!ready || addr == 0) return false;
  Wire.requestFrom((int)addr, 2);
  if (Wire.available() < 2) return false;

  const uint16_t raw = (uint16_t(Wire.read()) << 8) | uint16_t(Wire.read());
  luxOut = raw / 1.2f;
  return true;
}

} // namespace LightSensor
