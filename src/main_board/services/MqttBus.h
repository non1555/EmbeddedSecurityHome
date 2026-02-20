#pragma once

#include <Arduino.h>

#include "app/Commands.h"
#include "app/Events.h"
#include "app/SystemState.h"

class MqttBus {
public:
  struct Stats {
    uint32_t pubDrops = 0;
    uint32_t cmdDrops = 0;
    uint32_t storeDrops = 0;
    uint32_t tickOverruns = 0;
    uint32_t storeDepth = 0;
    uint32_t cmdQueueDepth = 0;
    uint32_t pubQueueDepth = 0;
  };

  void begin();
  void update(uint32_t nowMs);

  void publishEvent(const Event& e, const SystemState& st, const Command& cmd);
  void publishStatus(const SystemState& st, const char* reason);
  void publishAck(const char* cmd, bool ok, const char* detail);

  bool pollCommand(String& outPayload);

  void setSensorTelemetry(uint32_t drops, uint32_t depth);
  Stats stats() const;

private:
  class Impl;
  Impl* impl_ = nullptr;
  bool useRtos_ = false;
};
