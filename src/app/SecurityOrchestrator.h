#pragma once

#include <Arduino.h>

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "app/Config.h"
#include "app/Events.h"
#include "app/HardwareConfig.h"
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
#if SERVO_COUNT >= 2
  Servo servo2_{HwCfg::PIN_SERVO2, 2, 2, 10, 90};
#endif
  Logger logger_;
  Notify notifySvc_;
  Actuators acts_{&buzzer_, &servo1_,
#if SERVO_COUNT >= 2
  &servo2_
#else
  nullptr
#endif
  };

  void applyDecision(const Event& e);
  void printEventDecision(const Event& e, const Decision& d) const;
  void processRemoteCommand(const String& payload);
  bool processManualActuatorEvent(const Event& e);
  bool processDoorHoldWarnSilenceEvent(const Event& e);
  void startDoorUnlockSession(uint32_t nowMs);
  void clearDoorUnlockSession(bool stopBuzzer);
  void updateDoorUnlockSession(uint32_t nowMs);

  bool doorUnlockSessionActive_ = false;
  bool doorSessionSawOpen_ = false;
  bool doorWasOpenLastTick_ = false;
  bool doorHoldWarnActive_ = false;
  bool doorHoldWarnSilenced_ = false;
  uint32_t doorUnlockDeadlineMs_ = 0;
  uint32_t doorOpenSinceMs_ = 0;
  uint32_t nextDoorWarnMs_ = 0;
};
