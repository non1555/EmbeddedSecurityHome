#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "core_services/MqttService.h"
#include "core_services/NvsStorage.h"
#include "configuration_shared_types/Types.h"
#include "hardware_abstraction_layer/Actuators.h"

namespace ApplicationLogicLayer {

class EventCollector;
class RuleEngine;

class SystemContext {
public:
  bool begin(); // Initializes services, RTOS primitives, actuator layer, and persisted mode. Params: none.
  void bindCollector(EventCollector* collector); // Binds event collector for sensor snapshot and display updates. Params: collector=pointer to collector instance.

  bool pollEvent(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls one pending event from remote commands or collector input. Params: nowMs=current timestamp in ms, out=event output.
  bool pollRemoteEvent(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Polls remote MQTT command and maps to event when applicable. Params: nowMs=current timestamp in ms, out=event output.
  void applyDecision(const ConfigurationSharedTypes::Event& event, const ConfigurationSharedTypes::Decision& decision); // Applies rule-engine decision to state, actuators, NVS, and MQTT reports. Params: event=source event, decision=rule result.
  void updateActuators(uint32_t nowMs, const RuleEngine& engine); // Runs periodic actuator updates, auto-arm tick, display refresh, and periodic status. Params: nowMs=current timestamp in ms, engine=rule engine for tick logic.
  void mqttTick(uint32_t nowMs); // Runs MQTT service loop for reconnect, rx, and publish flush from SecurityTask tick. Params: nowMs=current timestamp in ms.
  bool isMqttConnected(); // Returns current MQTT link status for task-level flow decisions. Params: none.

  void handleSilenceRequest(); // Handles keypad silence event for current warning session. Params: none.
  void handleHelpRequest(const ConfigurationSharedTypes::Event& event); // Handles keypad emergency-help event, raises alarm, and publishes help notification. Params: event=help event payload.
  void handleManualToggle(const ConfigurationSharedTypes::Event& event); // Handles local physical door/window toggle button and publishes resulting manual status. Params: event=manual toggle event payload.

  ConfigurationSharedTypes::SystemState& state(); // Returns mutable global system state. Params: none.
  const ConfigurationSharedTypes::SystemState& state() const; // Returns read-only global system state. Params: none.

private:
  struct DoorSession {
    bool active = false;
    bool sawOpen = false;
    bool lastDoorOpen = false;
    bool holdWarnSilenced = false;
    uint32_t unlockDeadlineMs = 0;
    uint32_t openWarnAtMs = 0;
    uint32_t closeLockAtMs = 0;
    uint32_t nextWarnMs = 0;
  };

  struct PendingDoorLockStatus {
    bool active = false;
    char reason[32] = {};
  };

  HardwareAbstractionLayer::Actuators actuators_{};
  CoreServices::MqttService mqtt_{};
  CoreServices::NvsStorage nvs_{};

  ConfigurationSharedTypes::SystemState state_{};
  DoorSession doorSession_{};
  PendingDoorLockStatus pendingDoorLockStatus_{};
  EventCollector* collector_ = nullptr;

  QueueHandle_t commandQueue_ = nullptr;
  uint32_t nextStatusAtMs_ = 0;

  static bool reached_(uint32_t nowMs, uint32_t targetMs); // Checks signed wrap-safe time reach condition. Params: nowMs=current timestamp in ms, targetMs=target time in ms.
  static void copyText_(char* out, size_t outLen, const char* in); // Safely copies c-string into bounded char buffer. Params: out=destination buffer, outLen=buffer size, in=source text.
  static String normalize_(String text); // Trims and lowercases command text for parsing. Params: text=input command string.
  static ConfigurationSharedTypes::Mode sanitizeBaseMode_(ConfigurationSharedTypes::Mode mode); // Normalizes base mode to disarm/away only. Params: mode=input mode value.

  void applyActiveModeFromBase_(); // Applies active mode from latest_mode and is_night override. Params: none.
  void persistModeState_(); // Persists latest_mode and is_night to NVS storage. Params: none.

  bool pollRemoteCommand_(uint32_t nowMs, ConfigurationSharedTypes::Event& out); // Consumes one or more remote MQTT commands and may emit a mapped event. Params: nowMs=current timestamp in ms, out=event output.
  void syncSnapshot_(); // Synchronizes open/locked fields from sensors and actuator states. Params: none.

  void startDoorSession_(uint32_t nowMs, bool doorOpen); // Starts unlock session timers for auto-lock workflow. Params: nowMs=current timestamp in ms, doorOpen=current door-open state.
  void clearDoorSession_(bool silenceBuzzer); // Clears unlock session state and optionally silences buzzer. Params: silenceBuzzer=true to stop buzzer.
  void updateDoorSession_(uint32_t nowMs, bool doorOpen); // Updates unlock-session countdown and warning/autolock behavior. Params: nowMs=current timestamp in ms, doorOpen=current door-open state.

  void countdownView_(uint32_t nowMs,
                      bool& active,
                      uint32_t& deadlineMs,
                      uint32_t& warnBeforeMs) const; // Computes countdown data for OLED rendering. Params: nowMs=current timestamp in ms, active=output active flag, deadlineMs=output countdown end time, warnBeforeMs=output warning threshold.

  void triggerWarn_(const char* reason); // Raises warning buzzer/state and publishes a status reason. Params: reason=warning reason tag.
  void enqueueStatus_(const char* reason); // Queues status message for MQTT publication. Params: reason=status reason tag.
  void enqueueEvent_(const ConfigurationSharedTypes::Event& event, const char* flag); // Queues event message for MQTT publication. Params: event=event payload, flag=event flag text.
  void enqueueAck_(const char* command, bool ok, const char* detail); // Queues command acknowledgement for MQTT publication. Params: command=command text, ok=ack success flag, detail=extra detail.
};

} // namespace ApplicationLogicLayer
