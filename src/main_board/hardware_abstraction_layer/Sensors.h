#pragma once

#include <Arduino.h>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/Types.h"

namespace HardwareAbstractionLayer {

class Sensors {
public:
  void begin(); // Initializes GPIO directions and baseline sensor states. Params: none.
  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls all inputs in fixed order and emits first event found. Params: nowMs=current timestamp in ms, out=event output.
  bool isDoorOpen() const; // Returns debounced door reed-open state. Params: none.
  bool isWindowOpen() const; // Returns debounced window reed-open state. Params: none.
  void printSerialHelp() const; // Prints supported serial command mapping. Params: none.

private:
  struct ReedState {
    bool stableOpen = false;
    bool lastRaw = false;
    bool firedOpen = false;
    uint32_t lastFlipMs = 0;
  };

  struct UltrasonicState {
    uint8_t trigPin = ConfigurationSharedTypes::Config::PIN_UNUSED;
    uint8_t echoPin = ConfigurationSharedTypes::Config::PIN_UNUSED;
    bool inside = false;
    uint32_t nextSampleMs = 0;
    uint32_t lastFireMs = 0;
  };

  ReedState doorReed_{};
  ReedState windowReed_{};

  bool pirLast_[3] = {false, false, false};
  uint32_t pirLastFireMs_[3] = {0, 0, 0};

  bool vibLast_ = false;
  uint32_t vibLastFireMs_ = 0;

  UltrasonicState us_[3]{};
  uint8_t usRoundRobinIdx_ = 0;

  char serialBuf_[48] = {0};
  uint8_t serialLen_ = 0;
  uint32_t serialLastByteMs_ = 0;

  static bool reached_(uint32_t nowMs, uint32_t targetMs); // Checks signed wrap-safe time reach condition. Params: nowMs=current timestamp in ms, targetMs=target time in ms.
  static bool pinConfigured_(uint8_t pin); // Checks whether pin is valid and not PIN_UNUSED. Params: pin=GPIO number.
  static uint8_t pirPin_(uint8_t idx); // Maps PIR index to configured GPIO pin. Params: idx=PIR index (0..2).

  int readUltrasonicCm_(const UltrasonicState& us, uint32_t timeoutUs = ConfigurationSharedTypes::Config::US_ECHO_TIMEOUT_US) const; // Reads ultrasonic distance in cm (returns 999 when no echo in timeout window). Params: us=ultrasonic channel state, timeoutUs=pulse timeout in microseconds.

  bool pollDoor_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls door reed sensor with debounce and edge detection. Params: nowMs=current timestamp in ms, out=event output.
  bool pollWindow_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls window reed sensor with debounce and edge detection. Params: nowMs=current timestamp in ms, out=event output.
  bool pollPir_(uint32_t nowMs, uint8_t idx, ConfigurationSharedTypes::Event& out); // Polls one PIR channel with cooldown on rising edge. Params: nowMs=current timestamp in ms, idx=PIR index (0..2), out=event output.
  bool pollUltrasonic_(uint32_t nowMs, uint8_t idx, ConfigurationSharedTypes::Event& out); // Polls one ultrasonic chokepoint channel with hysteresis/cooldown. Params: nowMs=current timestamp in ms, idx=ultrasonic index (0..2), out=event output.
  bool pollVibration_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls vibration sensor with cooldown on rising edge. Params: nowMs=current timestamp in ms, out=event output.

  bool pollSerial_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls serial input buffer and emits parsed test event. Params: nowMs=current timestamp in ms, out=event output.
  bool parseSerialChar_(char c, uint32_t nowMs, ConfigurationSharedTypes::Event& out) const; // Parses one-character serial shortcut command. Params: c=input char, nowMs=current timestamp in ms, out=event output.
  bool parseSerialCode_(uint16_t code, uint32_t nowMs, ConfigurationSharedTypes::Event& out) const; // Parses numeric serial command code. Params: code=input command code, nowMs=current timestamp in ms, out=event output.
  bool parseSerialToken_(const String& token, uint32_t nowMs, ConfigurationSharedTypes::Event& out) const; // Parses generic serial token into an event. Params: token=input text token, nowMs=current timestamp in ms, out=event output.
};

} // namespace HardwareAbstractionLayer
