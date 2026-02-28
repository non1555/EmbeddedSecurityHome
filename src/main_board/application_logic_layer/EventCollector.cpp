#include "application_logic_layer/EventCollector.h"

namespace ApplicationLogicLayer {

void EventCollector::begin() {
  Wire.begin(ConfigurationSharedTypes::Config::PIN_I2C_SDA,
             ConfigurationSharedTypes::Config::PIN_I2C_SCL);
  sensors_.begin();
  keypad_.begin();
  display_.begin();
  display_.showCode(keypad_.buffer(), keypad_.length());
}

bool EventCollector::poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (pollKeypad(nowMs, out)) return true;
  if (keypad_.consumeInputActivity()) return false;
  return pollSensorsOrSerial(nowMs, out);
}

bool EventCollector::pollKeypad(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  const bool fired = keypad_.poll(nowMs, out);
  display_.showCode(keypad_.buffer(), keypad_.length());

  bool ok = false;
  if (keypad_.consumeSubmitResult(ok)) {
    display_.showSubmitResult(ok);
  }

  display_.tick(nowMs);
  return fired;
}

bool EventCollector::pollSensorsOrSerial(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  return sensors_.poll(nowMs, out);
}

bool EventCollector::isDoorOpen() const {
  return sensors_.isDoorOpen();
}

bool EventCollector::isWindowOpen() const {
  return sensors_.isWindowOpen();
}

void EventCollector::updateDisplay(uint32_t nowMs,
                                   bool doorLocked,
                                   bool doorOpen,
                                   bool countdownActive,
                                   uint32_t countdownDeadlineMs,
                                   uint32_t countdownWarnBeforeMs) {
  display_.updateDoorStatus(nowMs,
                            doorLocked,
                            doorOpen,
                            countdownActive,
                            countdownDeadlineMs,
                            countdownWarnBeforeMs);
}

void EventCollector::printSerialHelp() const {
  sensors_.printSerialHelp();
}

} // namespace ApplicationLogicLayer
