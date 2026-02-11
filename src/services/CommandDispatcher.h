#pragma once
#include <Arduino.h>

#include "app/Commands.h"
#include "app/SystemState.h"

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "services/Notify.h"
#include "services/Logger.h"

struct Actuators {
  Buzzer* buzzer = nullptr;
  Servo* servo1 = nullptr;
  Servo* servo2 = nullptr;

  Actuators() = default;
  constexpr Actuators(Buzzer* b, Servo* s1, Servo* s2)
  : buzzer(b), servo1(s1), servo2(s2) {}
};

void applyCommand(const Command& cmd, const SystemState& st, Actuators& acts, Notify* notify = nullptr, Logger* logger = nullptr);
