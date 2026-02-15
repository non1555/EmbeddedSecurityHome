#include "MqttBus.h"

#include <cstring>

#include "rtos/Queues.h"
#include "rtos/Tasks.h"
#include "services/MqttClient.h"

namespace {
MqttClient gClient;
String gPendingCmd;
bool gHasPendingCmd = false;

void onDirectCommand(const String&, const String& payload) {
  gPendingCmd = payload;
  gHasPendingCmd = true;
}
}

void MqttBus::begin() {
  if (!impl_) impl_ = reinterpret_cast<Impl*>(1); // initialized marker
  RtosTasks::attachMqtt(&gClient);
  RtosTasks::startIfReady();
  if (!RtosQueues::mqttCmdQ) gClient.begin(onDirectCommand);
}

void MqttBus::update(uint32_t nowMs) {
  if (!RtosQueues::mqttCmdQ) gClient.update(nowMs);
}

void MqttBus::publishEvent(const Event& e, const SystemState& st, const Command& cmd) {
  if (!RtosQueues::mqttPubQ) {
    gClient.publishEvent(e, st, cmd);
    return;
  }
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::event;
  msg.e = e;
  msg.st = st;
  msg.cmd = cmd;
  RtosTasks::enqueuePublish(msg);
}

void MqttBus::publishStatus(const SystemState& st, const char* reason) {
  if (!RtosQueues::mqttPubQ) {
    gClient.publishStatus(st, reason);
    return;
  }
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::status;
  msg.st = st;
  if (reason) {
    std::strncpy(msg.text1, reason, sizeof(msg.text1) - 1);
    msg.text1[sizeof(msg.text1) - 1] = '\0';
  }
  RtosTasks::enqueuePublish(msg);
}

void MqttBus::publishAck(const char* cmd, bool ok, const char* detail) {
  if (!RtosQueues::mqttPubQ) {
    gClient.publishAck(cmd, ok, detail);
    return;
  }
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
}

bool MqttBus::pollCommand(String& outPayload) {
  outPayload = "";
  if (RtosQueues::mqttCmdQ) {
    RtosQueues::CmdMsg msg{};
    if (!RtosTasks::dequeueCommand(msg)) return false;
    outPayload = String(msg.payload);
    return true;
  }
  if (!gHasPendingCmd) return false;
  outPayload = gPendingCmd;
  gHasPendingCmd = false;
  return true;
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
  out.pubQueueDepth = RtosQueues::mqttPubQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttPubQ) : 0;
  out.cmdQueueDepth = RtosQueues::mqttCmdQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttCmdQ) : 0;
  return out;
}
