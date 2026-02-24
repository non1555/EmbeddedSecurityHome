#include "hardware_abstraction_layer/Actuators.h"

namespace {
constexpr uint8_t kBuzzerChannel = 0;
constexpr uint8_t kBuzzerResolutionBits = 10;
constexpr uint8_t kServoResolutionBits = 16;

uint32_t servoDutyFromDegrees(uint8_t deg) {
  if (deg > 180) deg = 180;
  constexpr uint16_t minUs = 500;
  constexpr uint16_t maxUs = 2500;
  const uint32_t pulseUs = (uint32_t)minUs + ((uint32_t)(maxUs - minUs) * deg) / 180u;
  const uint32_t maxDuty = (1u << kServoResolutionBits) - 1u;
  return (pulseUs * maxDuty) / 20000u;
}
} // namespace

namespace HardwareAbstractionLayer {

bool Actuators::reached_(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

bool Actuators::pinConfigured_(uint8_t pin) {
  return pin != ConfigurationSharedTypes::Config::PIN_UNUSED;
}

void Actuators::begin() {
  beginBuzzer_();

  setupServo_(doorServo_,
              ConfigurationSharedTypes::Config::PIN_SERVO_DOOR,
              1,
              ConfigurationSharedTypes::Config::DOOR_LOCK_DEG,
              ConfigurationSharedTypes::Config::DOOR_UNLOCK_DEG);

  setupServo_(windowServo_,
              ConfigurationSharedTypes::Config::PIN_SERVO_WINDOW,
              2,
              ConfigurationSharedTypes::Config::WINDOW_LOCK_DEG,
              ConfigurationSharedTypes::Config::WINDOW_UNLOCK_DEG);

  silence();
}

void Actuators::update(uint32_t nowMs) {
  updateBuzzer_(nowMs);
  updateServo_(doorServo_, nowMs);
  updateServo_(windowServo_, nowMs);
}

void Actuators::warn() {
  buzzerMode_ = BuzzerMode::warn;
  buzzerToneOn_ = false;
  buzzerNextMs_ = 0;
  buzzerStep_ = 0;
}

void Actuators::alert() {
  buzzerMode_ = BuzzerMode::alert;
  buzzerToneOn_ = false;
  buzzerNextMs_ = 0;
  buzzerStep_ = 0;
}

void Actuators::silence() {
  buzzerMode_ = BuzzerMode::idle;
  buzzerToneOn_ = false;
  buzzerNextMs_ = 0;
  buzzerStep_ = 0;
  setTone_(false, 0);
}

void Actuators::lockDoor() {
  doorServo_.targetDeg = doorServo_.lockDeg;
  doorServo_.nextMoveMs = 0;
}

void Actuators::unlockDoor() {
  doorServo_.targetDeg = doorServo_.unlockDeg;
  doorServo_.nextMoveMs = 0;
}

void Actuators::lockWindow() {
  windowServo_.targetDeg = windowServo_.lockDeg;
  windowServo_.nextMoveMs = 0;
}

void Actuators::unlockWindow() {
  windowServo_.targetDeg = windowServo_.unlockDeg;
  windowServo_.nextMoveMs = 0;
}

void Actuators::lockAll() {
  lockDoor();
  lockWindow();
}

void Actuators::unlockAll() {
  unlockDoor();
  unlockWindow();
}

bool Actuators::isDoorLocked() const {
  return doorServo_.currentDeg == doorServo_.lockDeg;
}

bool Actuators::isWindowLocked() const {
  return windowServo_.currentDeg == windowServo_.lockDeg;
}

void Actuators::beginBuzzer_() {
  if (!pinConfigured_(ConfigurationSharedTypes::Config::PIN_BUZZER)) return;
  ledcSetup(kBuzzerChannel, 2000, kBuzzerResolutionBits);
  ledcAttachPin(ConfigurationSharedTypes::Config::PIN_BUZZER, kBuzzerChannel);
  ledcWrite(kBuzzerChannel, 0);
}

void Actuators::setTone_(bool on, uint32_t hz) {
  if (!pinConfigured_(ConfigurationSharedTypes::Config::PIN_BUZZER)) return;
  if (!on || hz == 0) {
    ledcWrite(kBuzzerChannel, 0);
    buzzerToneOn_ = false;
    return;
  }

  ledcSetup(kBuzzerChannel, hz, kBuzzerResolutionBits);
  const uint32_t maxDuty = (1u << kBuzzerResolutionBits) - 1u;
  ledcWrite(kBuzzerChannel, maxDuty / 2u);
  buzzerToneOn_ = true;
}

void Actuators::updateBuzzer_(uint32_t nowMs) {
  if (buzzerMode_ == BuzzerMode::idle) return;
  if (buzzerNextMs_ != 0 && !reached_(nowMs, buzzerNextMs_)) return;

  if (buzzerMode_ == BuzzerMode::warn) {
    if (!buzzerToneOn_) {
      setTone_(true, 2200);
      buzzerNextMs_ = nowMs + 180;
    } else {
      setTone_(false, 0);
      buzzerNextMs_ = nowMs + 220;
      ++buzzerStep_;
      if (buzzerStep_ >= 6) {
        silence();
      }
    }
    return;
  }

  if (!buzzerToneOn_) {
    setTone_(true, 3200);
    buzzerNextMs_ = nowMs + 200;
  } else {
    setTone_(false, 0);
    buzzerNextMs_ = nowMs + 120;
  }
}

void Actuators::setupServo_(ServoState& servo,
                            uint8_t pin,
                            uint8_t channel,
                            uint8_t lockDeg,
                            uint8_t unlockDeg) {
  servo.pin = pin;
  servo.channel = channel;
  servo.lockDeg = lockDeg;
  servo.unlockDeg = unlockDeg;
  servo.currentDeg = unlockDeg;
  servo.targetDeg = unlockDeg;
  servo.nextMoveMs = 0;

  if (!pinConfigured_(servo.pin)) return;

  ledcSetup(servo.channel, 50, kServoResolutionBits);
  ledcAttachPin(servo.pin, servo.channel);
  writeServo_(servo, servo.currentDeg);
}

void Actuators::writeServo_(const ServoState& servo, uint8_t deg) const {
  if (!pinConfigured_(servo.pin)) return;
  ledcWrite(servo.channel, servoDutyFromDegrees(deg));
}

void Actuators::updateServo_(ServoState& servo, uint32_t nowMs) {
  if (!pinConfigured_(servo.pin)) return;
  if (servo.currentDeg == servo.targetDeg) return;
  if (servo.nextMoveMs != 0 && !reached_(nowMs, servo.nextMoveMs)) return;

  if (servo.currentDeg < servo.targetDeg) {
    ++servo.currentDeg;
  } else {
    --servo.currentDeg;
  }

  writeServo_(servo, servo.currentDeg);
  servo.nextMoveMs = nowMs + 15;
}

} // namespace HardwareAbstractionLayer
