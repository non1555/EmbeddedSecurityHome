#pragma once

#include <Arduino.h>

#include "app/Events.h"
#include "sensors/ChokepointSensor.h"

class AsyncChokepoint {
public:
  explicit AsyncChokepoint(ChokepointSensor* sensor);

  void begin();
  bool poll(Event& outEvent, int& outCm);

  uint32_t dropCount() const;
  uint32_t queueDepth() const;

private:
  ChokepointSensor* sensor_ = nullptr;
};

