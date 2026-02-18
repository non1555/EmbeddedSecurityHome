#pragma once

#include <Arduino.h>

#include "drivers/UltrasonicDriver.h"
#include "rtos/Queues.h"
#include "sensors/ChokepointSensor.h"
#include "services/MqttClient.h"

namespace RtosTasks {

struct Stats {
  uint32_t pubDrops = 0;
  uint32_t cmdDrops = 0;
  uint32_t storeDrops = 0;
  uint32_t tickOverruns = 0;
  uint32_t storeDepth = 0;
  uint32_t sensorDrops = 0;
  uint32_t sensorDepth = 0;
};

void attachMqtt(MqttClient* client);
void attachChokepoint(ChokepointSensor* sensor);
void startIfReady();

void setSensorTelemetry(uint32_t drops, uint32_t depth);
Stats stats();

bool enqueuePublish(const RtosQueues::PublishMsg& msg);
bool dequeueCommand(RtosQueues::CmdMsg& out);
bool dequeueChokepoint(RtosQueues::ChokepointMsg& out);

} // namespace RtosTasks
