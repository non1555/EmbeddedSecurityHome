#include "AsyncChokepoint.h"

#include "rtos/Queues.h"
#include "rtos/Tasks.h"

AsyncChokepoint::AsyncChokepoint(ChokepointSensor* sensor)
: sensor_(sensor) {}

void AsyncChokepoint::begin() {
  RtosTasks::attachChokepoint(sensor_);
  RtosTasks::startIfReady();
}

bool AsyncChokepoint::poll(Event& outEvent, int& outCm) {
  RtosQueues::ChokepointMsg msg{};
  if (!RtosTasks::dequeueChokepoint(msg)) return false;
  outEvent = msg.e;
  outCm = msg.cm;
  return true;
}

uint32_t AsyncChokepoint::dropCount() const {
  return RtosTasks::stats().sensorDrops;
}

uint32_t AsyncChokepoint::queueDepth() const {
  return RtosTasks::stats().sensorDepth;
}
