#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/Types.h"
#include "hardware_abstraction_layer/DisplayManager.h"
#include "hardware_abstraction_layer/KeypadController.h"
#include "hardware_abstraction_layer/Sensors.h"

namespace ApplicationLogicLayer {

class EventCollector {
public:
  void begin(); // Initializes I2C, sensors, keypad, and display modules. Params: none.

  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls keypad first, then sensors/serial, and emits first detected event. Params: nowMs=current timestamp in ms, out=event output.
  bool pollKeypad(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls keypad and emits an event when input completes. Params: nowMs=current timestamp in ms, out=event output.
  bool pollSensorsOrSerial(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls sensors/serial and emits the next detected event. Params: nowMs=current timestamp in ms, out=event output.

  bool isDoorOpen() const; // Returns current debounced door-open state. Params: none.
  bool isWindowOpen() const; // Returns current debounced window-open state. Params: none.

  void updateDisplay(uint32_t nowMs,
                     bool doorLocked,
                     bool doorOpen,
                     bool countdownActive,
                     uint32_t countdownDeadlineMs,
                     uint32_t countdownWarnBeforeMs); // Updates OLED with lock/open state and countdown info. Params: nowMs=current timestamp in ms, doorLocked=door lock status, doorOpen=door open status, countdownActive=whether countdown is active, countdownDeadlineMs=countdown end time, countdownWarnBeforeMs=warning threshold.

  void printSerialHelp() const; // Prints supported serial test commands to Serial monitor. Params: none.

private:
  HardwareAbstractionLayer::Sensors sensors_;
  HardwareAbstractionLayer::KeypadController keypad_;
  HardwareAbstractionLayer::DisplayManager display_;
};

} // namespace ApplicationLogicLayer
