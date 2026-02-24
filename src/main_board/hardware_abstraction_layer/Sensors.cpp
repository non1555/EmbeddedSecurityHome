#include "hardware_abstraction_layer/Sensors.h"

namespace HardwareAbstractionLayer {

bool Sensors::reached_(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

bool Sensors::pinConfigured_(uint8_t pin) {
  return pin != ConfigurationSharedTypes::Config::PIN_UNUSED;
}

uint8_t Sensors::pirPin_(uint8_t idx) {
  if (idx == 0) return ConfigurationSharedTypes::Config::PIN_PIR_1;
  if (idx == 1) return ConfigurationSharedTypes::Config::PIN_PIR_2;
  return ConfigurationSharedTypes::Config::PIN_PIR_3;
}

void Sensors::begin() {
  if (pinConfigured_(ConfigurationSharedTypes::Config::PIN_REED_DOOR)) {
    pinMode(ConfigurationSharedTypes::Config::PIN_REED_DOOR, INPUT_PULLUP);
  }
  if (pinConfigured_(ConfigurationSharedTypes::Config::PIN_REED_WINDOW)) {
    pinMode(ConfigurationSharedTypes::Config::PIN_REED_WINDOW, INPUT_PULLUP);
  }
  if (pinConfigured_(ConfigurationSharedTypes::Config::PIN_PIR_1)) {
    pinMode(ConfigurationSharedTypes::Config::PIN_PIR_1, INPUT);
  }
  if (pinConfigured_(ConfigurationSharedTypes::Config::PIN_PIR_2)) {
    pinMode(ConfigurationSharedTypes::Config::PIN_PIR_2, INPUT);
  }
  if (pinConfigured_(ConfigurationSharedTypes::Config::PIN_PIR_3)) {
    pinMode(ConfigurationSharedTypes::Config::PIN_PIR_3, INPUT);
  }
  if (pinConfigured_(ConfigurationSharedTypes::Config::PIN_VIB)) {
    pinMode(ConfigurationSharedTypes::Config::PIN_VIB, INPUT_PULLUP);
  }

  us_[0].trigPin = ConfigurationSharedTypes::Config::PIN_US_TRIG_1;
  us_[0].echoPin = ConfigurationSharedTypes::Config::PIN_US_ECHO_1;
  us_[1].trigPin = ConfigurationSharedTypes::Config::PIN_US_TRIG_2;
  us_[1].echoPin = ConfigurationSharedTypes::Config::PIN_US_ECHO_2;
  us_[2].trigPin = ConfigurationSharedTypes::Config::PIN_US_TRIG_3;
  us_[2].echoPin = ConfigurationSharedTypes::Config::PIN_US_ECHO_3;

  for (uint8_t i = 0; i < 3; ++i) {
    if (pinConfigured_(us_[i].trigPin) && pinConfigured_(us_[i].echoPin)) {
      pinMode(us_[i].trigPin, OUTPUT);
      pinMode(us_[i].echoPin, INPUT);
      digitalWrite(us_[i].trigPin, LOW);
    }
  }

  const uint32_t nowMs = millis();

  doorReed_.stableOpen = pinConfigured_(ConfigurationSharedTypes::Config::PIN_REED_DOOR)
    ? (digitalRead(ConfigurationSharedTypes::Config::PIN_REED_DOOR) == HIGH)
    : false;
  doorReed_.lastRaw = doorReed_.stableOpen;
  doorReed_.firedOpen = doorReed_.stableOpen;
  doorReed_.lastFlipMs = nowMs;

  windowReed_.stableOpen = pinConfigured_(ConfigurationSharedTypes::Config::PIN_REED_WINDOW)
    ? (digitalRead(ConfigurationSharedTypes::Config::PIN_REED_WINDOW) == HIGH)
    : false;
  windowReed_.lastRaw = windowReed_.stableOpen;
  windowReed_.firedOpen = windowReed_.stableOpen;
  windowReed_.lastFlipMs = nowMs;

  pirLast_[0] = pinConfigured_(ConfigurationSharedTypes::Config::PIN_PIR_1)
    ? (digitalRead(ConfigurationSharedTypes::Config::PIN_PIR_1) == HIGH)
    : false;
  pirLast_[1] = pinConfigured_(ConfigurationSharedTypes::Config::PIN_PIR_2)
    ? (digitalRead(ConfigurationSharedTypes::Config::PIN_PIR_2) == HIGH)
    : false;
  pirLast_[2] = pinConfigured_(ConfigurationSharedTypes::Config::PIN_PIR_3)
    ? (digitalRead(ConfigurationSharedTypes::Config::PIN_PIR_3) == HIGH)
    : false;

  vibLast_ = pinConfigured_(ConfigurationSharedTypes::Config::PIN_VIB)
    ? (digitalRead(ConfigurationSharedTypes::Config::PIN_VIB) == HIGH)
    : false;

  for (uint8_t i = 0; i < 3; ++i) {
    us_[i].inside = false;
    us_[i].nextSampleMs = nowMs;
    us_[i].lastFireMs = 0;
  }
  usRoundRobinIdx_ = 0;

  serialLen_ = 0;
  serialLastByteMs_ = 0;
}

bool Sensors::poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (pollDoor_(nowMs, out)) return true;
  if (pollWindow_(nowMs, out)) return true;
  if (pollPir_(nowMs, 0, out)) return true;
  if (pollPir_(nowMs, 1, out)) return true;
  if (pollPir_(nowMs, 2, out)) return true;

  if (pollVibration_(nowMs, out)) return true;

  const uint8_t usIdx = usRoundRobinIdx_;
  usRoundRobinIdx_ = (uint8_t)((usRoundRobinIdx_ + 1u) % 3u);
  if (pollUltrasonic_(nowMs, usIdx, out)) return true;

  return pollSerial_(nowMs, out);
}

bool Sensors::isDoorOpen() const {
  return doorReed_.stableOpen;
}

bool Sensors::isWindowOpen() const {
  return windowReed_.stableOpen;
}

void Sensors::printSerialHelp() const {
  Serial.println("[SERIAL] 100 disarm, 102 arm_away, 103 arm_night");
  Serial.println("[SERIAL] 205 help, 206 code_ok, 207 code_bad, 208 entry_timeout");
  Serial.println("[SERIAL] 300 door_open, 301 window_open, 302 tamper, 303 vibration");
  Serial.println("[SERIAL] 310/311/312 motion, 320/321/322 chokepoint");
}

int Sensors::readUltrasonicCm_(const UltrasonicState& us, uint32_t timeoutUs) const {
  if (!pinConfigured_(us.trigPin) || !pinConfigured_(us.echoPin)) return -1;

  digitalWrite(us.trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(us.trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(us.trigPin, LOW);

  const unsigned long pulse = pulseIn(us.echoPin, HIGH, timeoutUs);
  if (pulse == 0) return 999;
  return (int)(pulse / 58UL);
}

bool Sensors::pollDoor_(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (!pinConfigured_(ConfigurationSharedTypes::Config::PIN_REED_DOOR)) return false;

  const bool rawOpen = (digitalRead(ConfigurationSharedTypes::Config::PIN_REED_DOOR) == HIGH);
  if (rawOpen != doorReed_.lastRaw) {
    doorReed_.lastRaw = rawOpen;
    doorReed_.lastFlipMs = nowMs;
  }

  if ((nowMs - doorReed_.lastFlipMs) < ConfigurationSharedTypes::Config::REED_DEBOUNCE_MS) {
    return false;
  }

  if (doorReed_.stableOpen != rawOpen) {
    doorReed_.stableOpen = rawOpen;
    if (!doorReed_.stableOpen) {
      doorReed_.firedOpen = false;
    }
  }

  if (doorReed_.stableOpen && !doorReed_.firedOpen) {
    doorReed_.firedOpen = true;
    out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::door_open, nowMs, 1};
    return true;
  }

  return false;
}

bool Sensors::pollWindow_(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (!pinConfigured_(ConfigurationSharedTypes::Config::PIN_REED_WINDOW)) return false;

  const bool rawOpen = (digitalRead(ConfigurationSharedTypes::Config::PIN_REED_WINDOW) == HIGH);
  if (rawOpen != windowReed_.lastRaw) {
    windowReed_.lastRaw = rawOpen;
    windowReed_.lastFlipMs = nowMs;
  }

  if ((nowMs - windowReed_.lastFlipMs) < ConfigurationSharedTypes::Config::REED_DEBOUNCE_MS) {
    return false;
  }

  if (windowReed_.stableOpen != rawOpen) {
    windowReed_.stableOpen = rawOpen;
    if (!windowReed_.stableOpen) {
      windowReed_.firedOpen = false;
    }
  }

  if (windowReed_.stableOpen && !windowReed_.firedOpen) {
    windowReed_.firedOpen = true;
    out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::window_open, nowMs, 2};
    return true;
  }

  return false;
}

bool Sensors::pollPir_(uint32_t nowMs, uint8_t idx, ConfigurationSharedTypes::Event& out) {
  const uint8_t pin = pirPin_(idx);
  if (!pinConfigured_(pin)) return false;

  const bool active = (digitalRead(pin) == HIGH);
  const bool rising = active && !pirLast_[idx];
  pirLast_[idx] = active;

  if (!rising) return false;
  if ((nowMs - pirLastFireMs_[idx]) < ConfigurationSharedTypes::Config::PIR_COOLDOWN_MS) return false;

  pirLastFireMs_[idx] = nowMs;
  out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::motion, nowMs, (uint8_t)(idx + 1)};
  return true;
}

bool Sensors::pollUltrasonic_(uint32_t nowMs, uint8_t idx, ConfigurationSharedTypes::Event& out) {
  if (!reached_(nowMs, us_[idx].nextSampleMs)) return false;
  us_[idx].nextSampleMs = nowMs + ConfigurationSharedTypes::Config::US_SAMPLE_MS;

  const int cm = readUltrasonicCm_(us_[idx], ConfigurationSharedTypes::Config::US_ECHO_TIMEOUT_US);
  if (cm < 0) return false;

  if (!us_[idx].inside && cm <= ConfigurationSharedTypes::Config::US_NEAR_CM) {
    if ((nowMs - us_[idx].lastFireMs) < ConfigurationSharedTypes::Config::US_COOLDOWN_MS) {
      return false;
    }
    us_[idx].inside = true;
    us_[idx].lastFireMs = nowMs;
    out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::chokepoint, nowMs, (uint8_t)(idx + 1)};
    return true;
  }

  if (us_[idx].inside && cm >= ConfigurationSharedTypes::Config::US_FAR_CM) {
    us_[idx].inside = false;
  }

  return false;
}

bool Sensors::pollVibration_(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  if (!pinConfigured_(ConfigurationSharedTypes::Config::PIN_VIB)) return false;

  const bool active = (digitalRead(ConfigurationSharedTypes::Config::PIN_VIB) == HIGH);
  const bool rising = active && !vibLast_;
  vibLast_ = active;

  if (!rising) return false;
  if ((nowMs - vibLastFireMs_) < ConfigurationSharedTypes::Config::VIB_COOLDOWN_MS) return false;

  vibLastFireMs_ = nowMs;
  out = ConfigurationSharedTypes::Event{ConfigurationSharedTypes::EventType::vib_spike, nowMs, 0};
  return true;
}

bool Sensors::pollSerial_(uint32_t nowMs, ConfigurationSharedTypes::Event& out) {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    serialLastByteMs_ = nowMs;

    if (c == '\n') {
      if (serialLen_ == 0) continue;
      serialBuf_[serialLen_] = '\0';
      const String token(serialBuf_);
      serialLen_ = 0;
      return parseSerialToken_(token, nowMs, out);
    }

    if (serialLen_ < (sizeof(serialBuf_) - 1)) {
      serialBuf_[serialLen_++] = c;
    }
  }

  if (serialLen_ > 0 && (nowMs - serialLastByteMs_) >= 40) {
    serialBuf_[serialLen_] = '\0';
    const String token(serialBuf_);
    serialLen_ = 0;
    return parseSerialToken_(token, nowMs, out);
  }

  return false;
}

bool Sensors::parseSerialChar_(char c, uint32_t nowMs, ConfigurationSharedTypes::Event& out) const {
  using namespace ConfigurationSharedTypes;

  if (c == '0') { out = Event{EventType::disarm, nowMs, 200}; return true; }
  if (c == '6') { out = Event{EventType::arm_away, nowMs, 200}; return true; }
  if (c == '9') { out = Event{EventType::arm_night, nowMs, 200}; return true; }
  if (c == '8') { out = Event{EventType::door_open, nowMs, 200}; return true; }
  if (c == '2') { out = Event{EventType::window_open, nowMs, 200}; return true; }
  if (c == '7') { out = Event{EventType::door_tamper, nowMs, 200}; return true; }
  if (c == '3') { out = Event{EventType::vib_spike, nowMs, 200}; return true; }
  if (c == '4') { out = Event{EventType::motion, nowMs, 1}; return true; }
  if (c == '5') { out = Event{EventType::chokepoint, nowMs, 1}; return true; }
  if (c == 'S' || c == 's') { out = Event{EventType::door_hold_warn_silence, nowMs, 200}; return true; }
  if (c == 'H' || c == 'h') { out = Event{EventType::keypad_help_request, nowMs, 200}; return true; }

  return false;
}

bool Sensors::parseSerialCode_(uint16_t code, uint32_t nowMs, ConfigurationSharedTypes::Event& out) const {
  using namespace ConfigurationSharedTypes;

  switch (code) {
    case 100: out = Event{EventType::disarm, nowMs, 200}; return true;
    case 102: out = Event{EventType::arm_away, nowMs, 200}; return true;
    case 103: out = Event{EventType::arm_night, nowMs, 200}; return true;
    case 205: out = Event{EventType::keypad_help_request, nowMs, 200}; return true;
    case 206: out = Event{EventType::door_code_unlock, nowMs, 200}; return true;
    case 207: out = Event{EventType::door_code_bad, nowMs, 200}; return true;
    case 208: out = Event{EventType::entry_timeout, nowMs, 200}; return true;
    case 300: out = Event{EventType::door_open, nowMs, 200}; return true;
    case 301: out = Event{EventType::window_open, nowMs, 200}; return true;
    case 302: out = Event{EventType::door_tamper, nowMs, 200}; return true;
    case 303: out = Event{EventType::vib_spike, nowMs, 200}; return true;
    case 310: out = Event{EventType::motion, nowMs, 1}; return true;
    case 311: out = Event{EventType::motion, nowMs, 2}; return true;
    case 312: out = Event{EventType::motion, nowMs, 3}; return true;
    case 320: out = Event{EventType::chokepoint, nowMs, 1}; return true;
    case 321: out = Event{EventType::chokepoint, nowMs, 2}; return true;
    case 322: out = Event{EventType::chokepoint, nowMs, 3}; return true;
    default: return false;
  }
}

bool Sensors::parseSerialToken_(const String& token,
                                uint32_t nowMs,
                                ConfigurationSharedTypes::Event& out) const {
  String text = token;
  text.trim();
  if (text.length() == 0) return false;

  if (text == "?" || text.equalsIgnoreCase("help")) {
    printSerialHelp();
    return false;
  }

  if (text.length() == 1) {
    return parseSerialChar_(text[0], nowMs, out);
  }

  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
  }

  return parseSerialCode_((uint16_t)text.toInt(), nowMs, out);
}

} // namespace HardwareAbstractionLayer
