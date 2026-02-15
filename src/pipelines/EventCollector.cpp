#include "pipelines/EventCollector.h"

EventCollector::EventCollector()
: us1_(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO),
  chokep1_(&us1_, 1, 35, 55, 200, 1500),
  us2_(HwCfg::PIN_US_TRIG_2, HwCfg::PIN_US_ECHO_2),
  chokep2_(&us2_, 2, 35, 55, 200, 1500),
  us3_(HwCfg::PIN_US_TRIG_3, HwCfg::PIN_US_ECHO_3),
  chokep3_(&us3_, 3, 35, 55, 200, 1500),
  reedDoor_(HwCfg::PIN_REED_1, 1, EventType::door_open, true, 80),
  reedWindow_(HwCfg::PIN_REED_2, 2, EventType::window_open, true, 80),
  pir1_(HwCfg::PIN_PIR_1, 1, 1500),
  pir2_(HwCfg::PIN_PIR_2, 2, 1500),
  pir3_(HwCfg::PIN_PIR_3, 3, 1500),
  vibCombined_(HwCfg::PIN_VIB_1, 0, 700),
  keypadDrv_(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60),
  keypadIn_(0) {}

void EventCollector::begin() {
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
  oled_.begin();
  keypadDrv_.begin();
  keypadIn_.begin();
  keypadIn_.setDoorCode("1234");
  pinMode(HwCfg::PIN_BTN_DOOR_TOGGLE, INPUT_PULLUP);
  pinMode(HwCfg::PIN_BTN_WINDOW_TOGGLE, INPUT_PULLUP);

  const bool doorPressed = (digitalRead(HwCfg::PIN_BTN_DOOR_TOGGLE) == LOW);
  doorToggleLastRawPressed_ = doorPressed;
  doorToggleStablePressed_ = doorPressed;
  doorToggleLastChangeMs_ = millis();

  const bool windowPressed = (digitalRead(HwCfg::PIN_BTN_WINDOW_TOGGLE) == LOW);
  windowToggleLastRawPressed_ = windowPressed;
  windowToggleStablePressed_ = windowPressed;
  windowToggleLastChangeMs_ = millis();

  reedDoor_.begin();
  reedWindow_.begin();
  pir1_.begin();
  pir2_.begin();
  pir3_.begin();
  vibCombined_.begin();
  us1_.begin();
  chokep1_.begin();
  us2_.begin();
  chokep2_.begin();
  us3_.begin();
  chokep3_.begin();
}

bool EventCollector::pollKeypad(uint32_t nowMs, Event& out) {
  const char k = keypadDrv_.update(nowMs);
  if (k == 'A') {
    out = {EventType::door_hold_warn_silence, nowMs, 0};
    return true;
  }
  if (k) {
    keypadIn_.feedKey(k, nowMs);
    oled_.showCode(keypadIn_.buf(), keypadIn_.len());

    KeypadInput::SubmitResult sr;
    if (keypadIn_.takeSubmitResult(sr)) {
      oled_.showResult(sr == KeypadInput::SubmitResult::ok);
    }
  }
  oled_.update(nowMs);
  return keypadIn_.poll(nowMs, out);
}

void EventCollector::updateOledStatus(uint32_t nowMs,
                                      bool doorLocked,
                                      bool doorOpen,
                                      bool countdownActive,
                                      uint32_t countdownDeadlineMs,
                                      uint32_t countdownWarnBeforeMs) {
  oled_.setDoorStatus(doorLocked,
                      doorOpen,
                      countdownActive,
                      countdownDeadlineMs,
                      countdownWarnBeforeMs);
  oled_.update(nowMs);
}

bool EventCollector::parseSerialEvent(char c, uint32_t nowMs, Event& out) const {
  if (c == '0') { out = {EventType::disarm, nowMs, 0}; return true; }
  if (c == '1') { out = {EventType::arm_night, nowMs, 0}; return true; }
  if (c == '6') { out = {EventType::arm_away, nowMs, 0}; return true; }
  if (c == '8') { out = {EventType::door_open, nowMs, 0}; return true; }
  if (c == '2') { out = {EventType::window_open, nowMs, 0}; return true; }
  if (c == '7') { out = {EventType::door_tamper, nowMs, 0}; return true; }
  if (c == '3') { out = {EventType::vib_spike, nowMs, 0}; return true; }
  if (c == '4') { out = {EventType::motion, nowMs, 0}; return true; }
  if (c == '5') { out = {EventType::chokepoint, nowMs, 0}; return true; }
  if (c == 'S' || c == 's') { out = {EventType::door_hold_warn_silence, nowMs, 0}; return true; }
  if (c == 'D' || c == 'd') { out = {EventType::manual_door_toggle, nowMs, 0}; return true; }
  if (c == 'W' || c == 'w') { out = {EventType::manual_window_toggle, nowMs, 0}; return true; }
  if (c == 'L' || c == 'l') { out = {EventType::manual_lock_request, nowMs, 0}; return true; }
  if (c == 'U' || c == 'u') { out = {EventType::manual_unlock_request, nowMs, 0}; return true; }
  return false;
}

bool EventCollector::readSerialEvent(uint32_t nowMs, Event& out) const {
  if (!Serial.available()) return false;
  const char c = (char)Serial.read();
  while (Serial.available()) {
    const char t = (char)Serial.read();
    if (t == '\n') break;
  }
  return parseSerialEvent(c, nowMs, out);
}

bool EventCollector::pollSensorOrSerial(uint32_t nowMs, Event& out) {
  if (pollManualButtons(nowMs, out)) return true;
  if (reedDoor_.poll(nowMs, out)) return true;
  if (reedWindow_.poll(nowMs, out)) return true;
  if (pir1_.poll(nowMs, out)) return true;
  if (pir2_.poll(nowMs, out)) return true;
  if (pir3_.poll(nowMs, out)) return true;
  if (vibCombined_.poll(nowMs, out)) return true;
  if (chokep1_.poll(nowMs, out)) return true;
  if (chokep2_.poll(nowMs, out)) return true;
  if (chokep3_.poll(nowMs, out)) return true;
  return readSerialEvent(nowMs, out);
}

bool EventCollector::pollManualButton(uint8_t pin,
                                      uint32_t nowMs,
                                      uint32_t debounceMs,
                                      bool& lastRawPressed,
                                      bool& stablePressed,
                                      uint32_t& lastChangeMs,
                                      EventType pressEvent,
                                      Event& out) {
  const bool rawPressed = (digitalRead(pin) == LOW);
  if (rawPressed != lastRawPressed) {
    lastRawPressed = rawPressed;
    lastChangeMs = nowMs;
  }

  if ((nowMs - lastChangeMs) < debounceMs) return false;
  if (rawPressed == stablePressed) return false;

  stablePressed = rawPressed;
  if (!stablePressed) return false;

  out = {pressEvent, nowMs, 0};
  return true;
}

bool EventCollector::pollManualButtons(uint32_t nowMs, Event& out) {
  static constexpr uint32_t kDebounceMs = 40;
  if (pollManualButton(HwCfg::PIN_BTN_DOOR_TOGGLE,
                       nowMs,
                       kDebounceMs,
                       doorToggleLastRawPressed_,
                       doorToggleStablePressed_,
                       doorToggleLastChangeMs_,
                       EventType::manual_door_toggle,
                       out)) {
    return true;
  }
  return pollManualButton(HwCfg::PIN_BTN_WINDOW_TOGGLE,
                          nowMs,
                          kDebounceMs,
                          windowToggleLastRawPressed_,
                          windowToggleStablePressed_,
                          windowToggleLastChangeMs_,
                          EventType::manual_window_toggle,
                          out);
}

bool EventCollector::isDoorOpen() const {
  return reedDoor_.isOpen();
}

bool EventCollector::isWindowOpen() const {
  return reedWindow_.isOpen();
}
