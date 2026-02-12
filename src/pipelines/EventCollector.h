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

#if !KEYPAD_USE_I2C_EXPANDER
#include "drivers/KeypadDriver.h"
#else
#include <Wire.h>
#include "drivers/I2CKeypadDriver.h"
#endif

class EventCollector {
public:
  EventCollector();

  void begin();
  bool pollKeypad(uint32_t nowMs, Event& out);
  bool pollSensorOrSerial(uint32_t nowMs, Event& out);
  bool isDoorOpen() const;
  bool isWindowOpen() const;

private:
  UltrasonicDriver us1_;
  ChokepointSensor chokep1_;
#if US_SENSOR_COUNT >= 2
  UltrasonicDriver us2_;
  ChokepointSensor chokep2_;
#endif
#if US_SENSOR_COUNT >= 3
  UltrasonicDriver us3_;
  ChokepointSensor chokep3_;
#endif

  ReedSensor reedDoor_;
  ReedSensor reedWindow_;
  PirSensor pir1_;
  PirSensor pir2_;
  PirSensor pir3_;
  VibrationSensor vib1_;
  VibrationSensor vib2_;

#if !KEYPAD_USE_I2C_EXPANDER
  KeypadDriver keypadDrv_;
#else
  I2CKeypadDriver keypadDrv_;
#endif
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
  bool readSerialEvent(uint32_t nowMs, Event& out) const;

  bool manualLockLastRawPressed_ = false;
  bool manualLockStablePressed_ = false;
  uint32_t manualLockLastChangeMs_ = 0;

  bool manualUnlockLastRawPressed_ = false;
  bool manualUnlockStablePressed_ = false;
  uint32_t manualUnlockLastChangeMs_ = 0;
};
