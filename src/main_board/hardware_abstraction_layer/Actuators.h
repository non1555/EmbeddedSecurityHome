#pragma once

#include <Arduino.h>

#include "configuration_shared_types/Config.h"

namespace HardwareAbstractionLayer {

class Actuators {
public:
  void begin(); // Initializes buzzer and servo outputs to known state. Params: none.
  void update(uint32_t nowMs); // Advances non-blocking buzzer/servo state machines. Params: nowMs=current timestamp in ms.

  void warn(); // Starts warning beep pattern. Params: none.
  void alert(); // Starts continuous alert siren pattern. Params: none.
  void silence(); // Stops buzzer output immediately. Params: none.

  void lockDoor(); // Commands door servo toward lock angle. Params: none.
  void unlockDoor(); // Commands door servo toward unlock angle. Params: none.
  void lockWindow(); // Commands window servo toward lock angle. Params: none.
  void unlockWindow(); // Commands window servo toward unlock angle. Params: none.
  void lockAll(); // Locks both door and window servos. Params: none.
  void unlockAll(); // Unlocks both door and window servos. Params: none.

  bool isDoorLocked() const; // Returns true when door servo is at lock angle. Params: none.
  bool isWindowLocked() const; // Returns true when window servo is at lock angle. Params: none.

private:
  enum class BuzzerMode : uint8_t {
    idle,
    warn,
    alert
  };

  struct ServoState {
    uint8_t pin = ConfigurationSharedTypes::Config::PIN_UNUSED;
    uint8_t channel = 0;
    uint8_t lockDeg = 0;
    uint8_t unlockDeg = 0;
    uint8_t currentDeg = 0;
    uint8_t targetDeg = 0;
    uint32_t nextMoveMs = 0;
  };

  BuzzerMode buzzerMode_ = BuzzerMode::idle;
  bool buzzerToneOn_ = false;
  uint32_t buzzerNextMs_ = 0;
  uint8_t buzzerStep_ = 0;

  ServoState doorServo_{};
  ServoState windowServo_{};

  static bool reached_(uint32_t nowMs, uint32_t targetMs); // Checks signed wrap-safe time reach condition. Params: nowMs=current timestamp in ms, targetMs=target time in ms.
  static bool pinConfigured_(uint8_t pin); // Checks whether pin is valid and not PIN_UNUSED. Params: pin=GPIO number.

  void beginBuzzer_(); // Configures LEDC channel for buzzer output. Params: none.
  void setTone_(bool on, uint32_t hz); // Enables/disables buzzer tone at frequency. Params: on=tone enable flag, hz=tone frequency in Hz.
  void updateBuzzer_(uint32_t nowMs); // Steps buzzer pattern scheduler. Params: nowMs=current timestamp in ms.

  void setupServo_(ServoState& servo,
                   uint8_t pin,
                   uint8_t channel,
                   uint8_t lockDeg,
                   uint8_t unlockDeg); // Initializes one servo profile and LEDC channel. Params: servo=servo state struct, pin=GPIO, channel=LEDC channel, lockDeg=lock angle, unlockDeg=unlock angle.
  void writeServo_(const ServoState& servo, uint8_t deg) const; // Writes one absolute servo angle. Params: servo=servo state struct, deg=target angle (0..180).
  void updateServo_(ServoState& servo, uint32_t nowMs); // Moves servo one step toward target angle. Params: servo=servo state struct, nowMs=current timestamp in ms.
};

} // namespace HardwareAbstractionLayer
