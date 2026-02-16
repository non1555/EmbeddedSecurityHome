#pragma once

#include <Arduino.h>

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "app/Config.h"
#include "services/Notify.h"

class DoorUnlockSession {
public:
  void start(uint32_t nowMs, bool doorOpen, const Config& cfg);
  void clear(bool stopBuzzer, Buzzer& buzzer);
  void update(uint32_t nowMs,
              bool doorOpen,
              const Config& cfg,
              Servo& doorServo,
              Buzzer& buzzer,
              Notify& notify);

  bool silenceHoldWarning(bool doorOpen, Buzzer& buzzer, Notify& notify);
  bool countdown(uint32_t nowMs,
                 bool doorLocked,
                 bool doorOpen,
                 const Config& cfg,
                 uint32_t& deadlineMs,
                 uint32_t& warnBeforeMs) const;

  bool isActive() const;

private:
  bool active_ = false;
  bool sawOpen_ = false;
  bool doorWasOpenLastTick_ = false;
  bool holdWarnActive_ = false;
  bool holdWarnSilenced_ = false;
  uint32_t unlockDeadlineMs_ = 0;
  uint32_t openWarnAtMs_ = 0;
  uint32_t closeLockAtMs_ = 0;
  uint32_t nextWarnMs_ = 0;
};

