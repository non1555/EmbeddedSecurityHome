#include "pipelines/EventCollector.h"

#ifndef DOOR_CODE
#define DOOR_CODE ""
#endif

namespace {
bool isValidDoorCode(const char* code) {
  if (!code) return false;
  if (strlen(code) != 4) return false;
  for (size_t i = 0; i < 4; ++i) {
    if (code[i] < '0' || code[i] > '9') return false;
  }
  return true;
}

bool pinConfigured(uint8_t pin) {
  return pin != HwCfg::PIN_UNUSED;
}
} // namespace

EventCollector::EventCollector()
: us1_(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO),
  chokep1_(&us1_, 1, 5, 10, 200, 1500),
  us2_(HwCfg::PIN_US_TRIG_2, HwCfg::PIN_US_ECHO_2),
  chokep2_(&us2_, 2, 5, 10, 200, 1500),
  us3_(HwCfg::PIN_US_TRIG_3, HwCfg::PIN_US_ECHO_3),
  chokep3_(&us3_, 3, 5, 10, 200, 1500),
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
  hasPendingSerialEvent_ = false;
  pendingSerialEvent_ = {};
  if (isValidDoorCode(DOOR_CODE)) {
    keypadIn_.setDoorCode(DOOR_CODE);
  } else {
    // Invalid or missing code => disable keypad unlock by using unreachable code.
    keypadIn_.setDoorCode("ABCD");
    Serial.println("[KEYPAD] WARN: DOOR_CODE invalid; keypad unlock disabled");
  }
  const uint32_t nowMs = millis();
  if (pinConfigured(HwCfg::PIN_BTN_DOOR_TOGGLE)) {
    pinMode(HwCfg::PIN_BTN_DOOR_TOGGLE, INPUT_PULLUP);
    const bool doorPressed = (digitalRead(HwCfg::PIN_BTN_DOOR_TOGGLE) == LOW);
    doorToggleLastRawPressed_ = doorPressed;
    doorToggleStablePressed_ = doorPressed;
  } else {
    doorToggleLastRawPressed_ = false;
    doorToggleStablePressed_ = false;
  }
  doorToggleLastChangeMs_ = nowMs;

  if (pinConfigured(HwCfg::PIN_BTN_WINDOW_TOGGLE)) {
    pinMode(HwCfg::PIN_BTN_WINDOW_TOGGLE, INPUT_PULLUP);
    const bool windowPressed = (digitalRead(HwCfg::PIN_BTN_WINDOW_TOGGLE) == LOW);
    windowToggleLastRawPressed_ = windowPressed;
    windowToggleStablePressed_ = windowPressed;
  } else {
    windowToggleLastRawPressed_ = false;
    windowToggleStablePressed_ = false;
  }
  windowToggleLastChangeMs_ = nowMs;

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
  if (k == 'B') {
    out = {EventType::keypad_help_request, nowMs, 0};
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
  static constexpr uint8_t kSerialSyntheticSrc = 250;
  if (c == '0') { out = {EventType::disarm, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '1') { out = {EventType::arm_night, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '6') { out = {EventType::arm_away, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '8') { out = {EventType::door_open, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '2') { out = {EventType::window_open, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '7') { out = {EventType::door_tamper, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '3') { out = {EventType::vib_spike, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '4') { out = {EventType::motion, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == '5') { out = {EventType::chokepoint, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == 'S' || c == 's') { out = {EventType::door_hold_warn_silence, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == 'H' || c == 'h') { out = {EventType::keypad_help_request, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == 'D' || c == 'd') { out = {EventType::manual_door_toggle, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == 'W' || c == 'w') { out = {EventType::manual_window_toggle, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == 'L' || c == 'l') { out = {EventType::manual_lock_request, nowMs, kSerialSyntheticSrc}; return true; }
  if (c == 'U' || c == 'u') { out = {EventType::manual_unlock_request, nowMs, kSerialSyntheticSrc}; return true; }
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
  Event first{};
  bool hasFirst = false;
  auto capture = [&](bool fired, const Event& e) {
    if (!fired || hasFirst) return;
    first = e;
    hasFirst = true;
  };

  Event e{};
  capture(pollManualButtons(nowMs, e), e);
  capture(reedDoor_.poll(nowMs, e), e);
  capture(reedWindow_.poll(nowMs, e), e);
  capture(pir1_.poll(nowMs, e), e);
  capture(pir2_.poll(nowMs, e), e);
  capture(pir3_.poll(nowMs, e), e);
  capture(vibCombined_.poll(nowMs, e), e);
  capture(chokep1_.poll(nowMs, e), e);
  capture(chokep2_.poll(nowMs, e), e);
  capture(chokep3_.poll(nowMs, e), e);

  // Keep serial as lowest priority. If another source already fired, queue one serial event
  // so it won't be dropped or starved indefinitely by busy sensors.
  if (hasPendingSerialEvent_) {
    if (!hasFirst) {
      out = pendingSerialEvent_;
      hasPendingSerialEvent_ = false;
      return true;
    }
  } else if (readSerialEvent(nowMs, e)) {
    if (!hasFirst) {
      first = e;
      hasFirst = true;
    } else {
      pendingSerialEvent_ = e;
      hasPendingSerialEvent_ = true;
    }
  }

  if (!hasFirst) return false;
  out = first;
  return true;
}

bool EventCollector::pollManualButton(uint8_t pin,
                                      uint32_t nowMs,
                                      uint32_t debounceMs,
                                      bool& lastRawPressed,
                                      bool& stablePressed,
                                      uint32_t& lastChangeMs,
                                      EventType pressEvent,
                                      Event& out) {
  if (!pinConfigured(pin)) return false;

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

void EventCollector::readHealth(uint32_t nowMs,
                                uint32_t pirStuckActiveMs,
                                uint32_t vibStuckActiveMs,
                                uint32_t ultrasonicOfflineMs,
                                uint16_t ultrasonicNoEchoThreshold,
                                HealthSnapshot& out) const {
  out = {};
  const bool us1Configured = pinConfigured(HwCfg::PIN_US_TRIG) && pinConfigured(HwCfg::PIN_US_ECHO);
  const bool us2Configured = pinConfigured(HwCfg::PIN_US_TRIG_2) && pinConfigured(HwCfg::PIN_US_ECHO_2);
  const bool us3Configured = pinConfigured(HwCfg::PIN_US_TRIG_3) && pinConfigured(HwCfg::PIN_US_ECHO_3);

  out.pir1_stuck_active = pir1_.isStuckActive(nowMs, pirStuckActiveMs);
  out.pir2_stuck_active = pir2_.isStuckActive(nowMs, pirStuckActiveMs);
  out.pir3_stuck_active = pir3_.isStuckActive(nowMs, pirStuckActiveMs);
  out.vib_stuck_active = vibCombined_.isStuckActive(nowMs, vibStuckActiveMs);
  out.us1_offline = us1Configured && chokep1_.isOffline(nowMs, ultrasonicOfflineMs, ultrasonicNoEchoThreshold);
  out.us2_offline = us2Configured && chokep2_.isOffline(nowMs, ultrasonicOfflineMs, ultrasonicNoEchoThreshold);
  out.us3_offline = us3Configured && chokep3_.isOffline(nowMs, ultrasonicOfflineMs, ultrasonicNoEchoThreshold);
}
