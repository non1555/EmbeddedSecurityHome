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

  bool poll(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls keypad first, then sensors/serial, and emits first detected event; keypad edits consume the tick before sensors run. Params: nowMs=current timestamp in ms, out=event output.
  bool pollKeypad(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls keypad and emits an event when input completes. Params: nowMs=current timestamp in ms, out=event output.
  bool pollSensorsOrSerial(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls manual buttons first, then sensors, and emits the next detected event. Params: nowMs=current timestamp in ms, out=event output.

  bool isDoorOpen() const; // Returns current debounced door-open state. Params: none.
  bool isWindowOpen() const; // Returns current debounced window-open state. Params: none.

  void updateDisplay(uint32_t nowMs,
                     bool doorLocked,
                     bool doorOpen,
                     bool countdownActive,
                     uint32_t countdownDeadlineMs,
                     uint32_t countdownWarnBeforeMs); // Updates OLED with lock/open state and countdown info. Params: nowMs=current timestamp in ms, doorLocked=door lock status, doorOpen=door open status, countdownActive=whether countdown is active, countdownDeadlineMs=countdown end time, countdownWarnBeforeMs=warning threshold.

private:
  bool pollManualButtons_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls local door/window toggle buttons with debounce and emits manual toggle event. Params: nowMs=current timestamp in ms, out=event output.
  bool pollManualButton_(uint8_t pin,
                         uint32_t nowMs,
                         bool& lastRawPressed,
                         bool& stablePressed,
                         uint32_t& lastChangeMs,
                         ConfigurationSharedTypes::EventType pressEvent,
                         ConfigurationSharedTypes::Event& out); // Polls one local manual button using edge debounce and emits event on stable press. Params: pin=GPIO number, nowMs=current timestamp in ms, lastRawPressed=raw input state cache, stablePressed=debounced state cache, lastChangeMs=last edge time, pressEvent=event to emit on press, out=event output.

  HardwareAbstractionLayer::Sensors sensors_;
  HardwareAbstractionLayer::KeypadController keypad_;
  HardwareAbstractionLayer::DisplayManager display_;
  bool doorToggleLastRawPressed_ = false;
  bool doorToggleStablePressed_ = false;
  uint32_t doorToggleLastChangeMs_ = 0;
  bool windowToggleLastRawPressed_ = false;
  bool windowToggleStablePressed_ = false;
  uint32_t windowToggleLastChangeMs_ = 0;
};

} // namespace ApplicationLogicLayer
