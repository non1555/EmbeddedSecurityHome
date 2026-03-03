#include "application_logic_layer/EventCollector.h"

namespace ApplicationLogicLayer {

void EventCollector::begin() {
  const uint32_t nowMs = millis();
  Wire.begin(ConfigurationSharedTypes::Config::PIN_I2C_SDA,
             ConfigurationSharedTypes::Config::PIN_I2C_SCL);
  pinMode(ConfigurationSharedTypes::Config::PIN_BTN_DOOR_TOGGLE, INPUT_PULLUP);
  pinMode(ConfigurationSharedTypes::Config::PIN_BTN_WINDOW_TOGGLE, INPUT_PULLUP);
  doorToggleLastRawPressed_ = (digitalRead(ConfigurationSharedTypes::Config::PIN_BTN_DOOR_TOGGLE) == LOW);
  doorToggleStablePressed_ = doorToggleLastRawPressed_;
  doorToggleLastChangeMs_ = nowMs;
  windowToggleLastRawPressed_ = (digitalRead(ConfigurationSharedTypes::Config::PIN_BTN_WINDOW_TOGGLE) == LOW);
  windowToggleStablePressed_ = windowToggleLastRawPressed_;
  windowToggleLastChangeMs_ = nowMs;
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
  if (pollManualButtons_(nowMs, out)) return true;
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

bool EventCollector::pollManualButtons_(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (pollManualButton_(ConfigurationSharedTypes::Config::PIN_BTN_DOOR_TOGGLE,
                        nowMs,
                        doorToggleLastRawPressed_,
                        doorToggleStablePressed_,
                        doorToggleLastChangeMs_,
                        ConfigurationSharedTypes::EventType::manual_door_toggle,
                        out)) {
    return true;
  }

  return pollManualButton_(ConfigurationSharedTypes::Config::PIN_BTN_WINDOW_TOGGLE,
                           nowMs,
                           windowToggleLastRawPressed_,
                           windowToggleStablePressed_,
                           windowToggleLastChangeMs_,
                           ConfigurationSharedTypes::EventType::manual_window_toggle,
                           out);
}

bool EventCollector::pollManualButton_(uint8_t pin,
                                       uint32_t nowMs,
                                       bool& lastRawPressed,
                                       bool& stablePressed,
                                       uint32_t& lastChangeMs,
                                       ConfigurationSharedTypes::EventType pressEvent,
                                       ConfigurationSharedTypes::Event& out) {
  static constexpr uint32_t kDebounceMs = 40;

  const bool rawPressed = (digitalRead(pin) == LOW);
  if (rawPressed != lastRawPressed) {
    lastRawPressed = rawPressed;
    lastChangeMs = nowMs;
  }

  if ((nowMs - lastChangeMs) < kDebounceMs) return false;
  if (rawPressed == stablePressed) return false;

  stablePressed = rawPressed;
  if (!stablePressed) return false;

  out = ConfigurationSharedTypes::Event{pressEvent, nowMs, 0};
  return true;
}

} // namespace ApplicationLogicLayer
