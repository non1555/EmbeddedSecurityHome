#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "app/Config.h"
#include "app/DoorUnlockSession.h"
#include "app/Events.h"
#include "app/HardwareConfig.h"
#include "app/ReplayGuard.h"
#include "app/RuleEngine.h"
#include "app/SystemState.h"
#include "pipelines/EventCollector.h"
#include "pipelines/EventGate.h"
#include "pipelines/TimeoutScheduler.h"
#include "services/CommandDispatcher.h"
#include "services/Logger.h"
#include "services/MqttBus.h"
#include "services/Notify.h"

class SecurityOrchestrator {
public:
  void begin();
  void tick(uint32_t nowMs);

private:
  RuleEngine engine_;
  SystemState state_;
  Config cfg_;

  EventCollector collector_;
  TimeoutScheduler timeoutScheduler_;
  MqttBus mqttBus_;

  Buzzer buzzer_{HwCfg::PIN_BUZZER, 0};
  Servo servo1_{HwCfg::PIN_SERVO1, 1, 1, 10, 90};
  Servo servo2_{HwCfg::PIN_SERVO2, 2, 2, 10, 90};
  Logger logger_;
  Notify notifySvc_;
  Actuators acts_{&buzzer_, &servo1_, &servo2_};

  void applyDecision(const Event& e);
  void printEventDecision(const Event& e, const Decision& d) const;
  void processRemoteCommand(const String& payload);
  bool processManualActuatorEvent(const Event& e);
  bool processDoorHoldWarnSilenceEvent(const Event& e);
  bool processModeEvent(const Event& e, const char* origin);
  bool acceptRemoteNonce(const String& nonce, uint32_t nowMs, bool persistMonotonicFloor);
  void updateSensorHealth(uint32_t nowMs);
  void startDoorUnlockSession(uint32_t nowMs);
  void clearDoorUnlockSession(bool stopBuzzer);
  void updateDoorUnlockSession(uint32_t nowMs);

  DoorUnlockSession doorSession_;

  uint8_t badDoorCodeAttempts_ = 0;
  uint32_t keypadLockoutUntilMs_ = 0;
  uint32_t lastKeypadLockoutNotifyMs_ = 0;

  bool servo1WasLocked_ = false;
  uint32_t nextStatusHeartbeatMs_ = 0;
  ReplayGuard remoteNonceGuard_;
  uint32_t nextSensorHealthCheckMs_ = 0;
  uint32_t lastSensorFaultNotifyMs_ = 0;
  bool sensorFaultActive_ = false;
  String sensorFaultDetail_;

  Preferences noncePref_;
  bool noncePrefReady_ = false;
  uint32_t lastRemoteNonce_ = 0;
};
