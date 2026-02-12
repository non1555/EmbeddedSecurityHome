#include "pipelines/EventCollector.h"

EventCollector::EventCollector()
: us1_(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO),
  chokep1_(&us1_, 1, 35, 55, 200, 1500),
#if US_SENSOR_COUNT >= 2
  us2_(HwCfg::PIN_US_TRIG_2, HwCfg::PIN_US_ECHO_2),
  chokep2_(&us2_, 2, 35, 55, 200, 1500),
#endif
#if US_SENSOR_COUNT >= 3
  us3_(HwCfg::PIN_US_TRIG_3, HwCfg::PIN_US_ECHO_3),
  chokep3_(&us3_, 3, 35, 55, 200, 1500),
#endif
  reedDoor_(HwCfg::PIN_REED_1, 1, EventType::door_open, true, 80),
  reedWindow_(HwCfg::PIN_REED_2, 2, EventType::window_open, true, 80),
  pir1_(HwCfg::PIN_PIR_1, 1, 1500),
  pir2_(HwCfg::PIN_PIR_2, 2, 1500),
  pir3_(HwCfg::PIN_PIR_3, 3, 1500),
  vib1_(HwCfg::PIN_VIB_1, 1, 600, 700),
  vib2_(HwCfg::PIN_VIB_2, 2, 600, 700),
#if !KEYPAD_USE_I2C_EXPANDER
  keypadDrv_(HwCfg::KP_ROWS, 4, HwCfg::KP_COLS, 4, HwCfg::KP_MAP, 60),
#else
  keypadDrv_(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60),
#endif
  keypadIn_(0) {}

void EventCollector::begin() {
#if KEYPAD_USE_I2C_EXPANDER
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
#endif
  keypadDrv_.begin();
  keypadIn_.begin();
  keypadIn_.setArmCode("1234");
  keypadIn_.setDisarmCode("0000");
  pinMode(HwCfg::PIN_BTN_MANUAL_LOCK, INPUT_PULLUP);
  pinMode(HwCfg::PIN_BTN_MANUAL_UNLOCK, INPUT_PULLUP);

  const bool lockPressed = (digitalRead(HwCfg::PIN_BTN_MANUAL_LOCK) == LOW);
  manualLockLastRawPressed_ = lockPressed;
  manualLockStablePressed_ = lockPressed;
  manualLockLastChangeMs_ = millis();

  const bool unlockPressed = (digitalRead(HwCfg::PIN_BTN_MANUAL_UNLOCK) == LOW);
  manualUnlockLastRawPressed_ = unlockPressed;
  manualUnlockStablePressed_ = unlockPressed;
  manualUnlockLastChangeMs_ = millis();

  reedDoor_.begin();
  reedWindow_.begin();
  pir1_.begin();
  pir2_.begin();
  pir3_.begin();
  vib1_.begin();
  vib2_.begin();
  us1_.begin();
  chokep1_.begin();
#if US_SENSOR_COUNT >= 2
  us2_.begin();
  chokep2_.begin();
#endif
#if US_SENSOR_COUNT >= 3
  us3_.begin();
  chokep3_.begin();
#endif
}

bool EventCollector::pollKeypad(uint32_t nowMs, Event& out) {
  const char k = keypadDrv_.update(nowMs);
  if (k == 'A') {
    out = {EventType::door_hold_warn_silence, nowMs, 0};
    return true;
  }
  if (k) keypadIn_.feedKey(k, nowMs);
  return keypadIn_.poll(nowMs, out);
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
  if (vib1_.poll(nowMs, out)) return true;
  if (vib2_.poll(nowMs, out)) return true;
  if (chokep1_.poll(nowMs, out)) return true;
#if US_SENSOR_COUNT >= 2
  if (chokep2_.poll(nowMs, out)) return true;
#endif
#if US_SENSOR_COUNT >= 3
  if (chokep3_.poll(nowMs, out)) return true;
#endif
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
  if (pollManualButton(HwCfg::PIN_BTN_MANUAL_LOCK,
                       nowMs,
                       kDebounceMs,
                       manualLockLastRawPressed_,
                       manualLockStablePressed_,
                       manualLockLastChangeMs_,
                       EventType::manual_lock_request,
                       out)) {
    return true;
  }
  return pollManualButton(HwCfg::PIN_BTN_MANUAL_UNLOCK,
                          nowMs,
                          kDebounceMs,
                          manualUnlockLastRawPressed_,
                          manualUnlockStablePressed_,
                          manualUnlockLastChangeMs_,
                          EventType::manual_unlock_request,
                          out);
}

bool EventCollector::isDoorOpen() const {
  return reedDoor_.isOpen();
}

bool EventCollector::isWindowOpen() const {
  return reedWindow_.isOpen();
}
