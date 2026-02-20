#pragma once

#include <Arduino.h>

#include "app/Events.h"
#include "app/HardwareConfig.h"
#include "drivers/UltrasonicDriver.h"
#include "sensors/ChokepointSensor.h"
#include "sensors/KeypadInput.h"
#include "sensors/PirSensor.h"
#include "sensors/ReedSensor.h"
#include "sensors/VibrationSensor.h"
#include "ui/OledCodeUi.h"

#include <Wire.h>
#include "drivers/I2CKeypadDriver.h"

class EventCollector {
public:
  struct HealthSnapshot {
    bool pir1_stuck_active = false;
    bool pir2_stuck_active = false;
    bool pir3_stuck_active = false;
    bool vib_stuck_active = false;
    bool us1_offline = false;
    bool us2_offline = false;
    bool us3_offline = false;
  };

  EventCollector();

  void begin();
  bool pollKeypad(uint32_t nowMs, Event& out);
  bool pollSensorOrSerial(uint32_t nowMs, Event& out);
  void printSerialHelp() const;
  bool isDoorOpen() const;
  bool isWindowOpen() const;
  void readHealth(uint32_t nowMs,
                  uint32_t pirStuckActiveMs,
                  uint32_t vibStuckActiveMs,
                  uint32_t ultrasonicOfflineMs,
                  uint16_t ultrasonicNoEchoThreshold,
                  HealthSnapshot& out) const;
  void updateOledStatus(uint32_t nowMs,
                        bool doorLocked,
                        bool doorOpen,
                        bool countdownActive,
                        uint32_t countdownDeadlineMs,
                        uint32_t countdownWarnBeforeMs);

private:
  UltrasonicDriver us1_;
  ChokepointSensor chokep1_;
  UltrasonicDriver us2_;
  ChokepointSensor chokep2_;
  UltrasonicDriver us3_;
  ChokepointSensor chokep3_;

  ReedSensor reedDoor_;
  ReedSensor reedWindow_;
  PirSensor pir1_;
  PirSensor pir2_;
  PirSensor pir3_;

  // Multiple vibration switches wired together into one input.
  VibrationSensor vibCombined_;

  OledCodeUi oled_{HwCfg::OLED_I2C_ADDR};

  I2CKeypadDriver keypadDrv_;
  KeypadInput keypadIn_;

  bool pollManualButton(uint8_t pin,
                        uint32_t nowMs,
                        uint32_t debounceMs,
                        bool& lastRawPressed,
                        bool& stablePressed,
                        uint32_t& lastChangeMs,
                        EventType pressEvent,
                        Event& out);
  bool pollManualButtons(uint32_t nowMs, Event& out);
  bool parseSerialEvent(char c, uint32_t nowMs, Event& out) const;
  bool parseSerialEvent(const String& token, uint32_t nowMs, Event& out) const;
  bool parseSerialCode(uint16_t code, uint32_t nowMs, Event& out) const;
  bool readSerialEvent(uint32_t nowMs, Event& out);

  bool doorToggleLastRawPressed_ = false;
  bool doorToggleStablePressed_ = false;
  uint32_t doorToggleLastChangeMs_ = 0;

  bool windowToggleLastRawPressed_ = false;
  bool windowToggleStablePressed_ = false;
  uint32_t windowToggleLastChangeMs_ = 0;

  bool hasPendingSerialEvent_ = false;
  Event pendingSerialEvent_{};
  char serialLineBuf_[48] = {0};
  uint8_t serialLineLen_ = 0;
  uint32_t serialLineLastByteMs_ = 0;
};
