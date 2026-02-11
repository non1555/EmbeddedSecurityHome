#include "MqttBus.h"

#include <cstring>

#include "rtos/Queues.h"
#include "rtos/Tasks.h"
#include "services/MqttClient.h"

namespace {
MqttClient gClient;
}

void MqttBus::begin() {
  if (!impl_) impl_ = reinterpret_cast<Impl*>(1); // initialized marker
  RtosTasks::attachMqtt(&gClient);
  RtosTasks::startIfReady();
}

void MqttBus::update(uint32_t nowMs) {
#if !defined(ARDUINO_ARCH_ESP32)
  gClient.update(nowMs);
#else
  (void)nowMs;
#endif
}

void MqttBus::publishEvent(const Event& e, const SystemState& st, const Command& cmd) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!RtosQueues::mqttPubQ) return;
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::event;
  msg.e = e;
  msg.st = st;
  msg.cmd = cmd;
  RtosTasks::enqueuePublish(msg);
#else
  gClient.publishEvent(e, st, cmd);
#endif
}

void MqttBus::publishStatus(const SystemState& st, const char* reason) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!RtosQueues::mqttPubQ) return;
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::status;
  msg.st = st;
  if (reason) {
    std::strncpy(msg.text1, reason, sizeof(msg.text1) - 1);
    msg.text1[sizeof(msg.text1) - 1] = '\0';
  }
  RtosTasks::enqueuePublish(msg);
#else
  gClient.publishStatus(st, reason);
#endif
}

void MqttBus::publishAck(const char* cmd, bool ok, const char* detail) {
#if defined(ARDUINO_ARCH_ESP32)
  if (!RtosQueues::mqttPubQ) return;
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::ack;
  msg.ok = ok;
  if (cmd) {
    std::strncpy(msg.text1, cmd, sizeof(msg.text1) - 1);
    msg.text1[sizeof(msg.text1) - 1] = '\0';
  }
  if (detail) {
    std::strncpy(msg.text2, detail, sizeof(msg.text2) - 1);
    msg.text2[sizeof(msg.text2) - 1] = '\0';
  }
  RtosTasks::enqueuePublish(msg);
#else
  gClient.publishAck(cmd, ok, detail);
#endif
}

bool MqttBus::pollCommand(String& outPayload) {
  outPayload = "";
#if defined(ARDUINO_ARCH_ESP32)
  RtosQueues::CmdMsg msg{};
  if (!RtosTasks::dequeueCommand(msg)) return false;
  outPayload = String(msg.payload);
  return true;
#else
  return false;
#endif
}

void MqttBus::setSensorTelemetry(uint32_t drops, uint32_t depth) {
  RtosTasks::setSensorTelemetry(drops, depth);
}

MqttBus::Stats MqttBus::stats() const {
  MqttBus::Stats out{};
  const auto s = RtosTasks::stats();
  out.pubDrops = s.pubDrops;
  out.cmdDrops = s.cmdDrops;
  out.storeDrops = s.storeDrops;
  out.tickOverruns = s.tickOverruns;
  out.storeDepth = s.storeDepth;
#if defined(ARDUINO_ARCH_ESP32)
  out.pubQueueDepth = RtosQueues::mqttPubQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttPubQ) : 0;
  out.cmdQueueDepth = RtosQueues::mqttCmdQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttCmdQ) : 0;
#endif
  return out;
}
