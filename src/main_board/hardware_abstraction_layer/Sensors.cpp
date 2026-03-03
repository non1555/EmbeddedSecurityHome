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
  return pollUltrasonic_(nowMs, usIdx, out);
}

bool Sensors::isDoorOpen() const {
  return doorReed_.stableOpen;
}

bool Sensors::isWindowOpen() const {
  return windowReed_.stableOpen;
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

} // namespace HardwareAbstractionLayer
