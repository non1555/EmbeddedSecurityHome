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
#if defined(ARDUINO_ARCH_ESP32)
  RtosQueues::ChokepointMsg msg{};
  if (!RtosTasks::dequeueChokepoint(msg)) return false;
  outEvent = msg.e;
  outCm = msg.cm;
  return true;
#else
  if (!sensor_) return false;
  const uint32_t nowMs = millis();
  if (!sensor_->poll(nowMs, outEvent)) return false;
  outCm = sensor_->lastCm();
  return true;
#endif
}

uint32_t AsyncChokepoint::dropCount() const {
  return RtosTasks::stats().sensorDrops;
}

uint32_t AsyncChokepoint::queueDepth() const {
#if defined(ARDUINO_ARCH_ESP32)
  return RtosTasks::stats().sensorDepth;
#else
  return 0;
#endif
}
