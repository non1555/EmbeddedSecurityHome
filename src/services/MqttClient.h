#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "app/Commands.h"
#include "app/Events.h"
#include "app/SystemState.h"

class MqttClient {
public:
  using CommandCallback = void (*)(const String& topic, const String& payload);

  MqttClient();

  void begin(CommandCallback cb = nullptr);
  void update(uint32_t nowMs);

  bool ready();
  bool publishEvent(const Event& e, const SystemState& st, const Command& cmd);
  bool publishStatus(const SystemState& st, const char* reason);
  bool publishAck(const char* cmd, bool ok, const char* detail);
  bool publishMetrics(
    uint32_t usDrops,
    uint32_t pubDrops,
    uint32_t cmdDrops,
    uint32_t storeDrops,
    uint32_t usQueueDepth,
    uint32_t pubQueueDepth,
    uint32_t cmdQueueDepth,
    uint32_t storeDepth
  );

private:
  static MqttClient* self_;

  WiFiClient wifiClient_;
  PubSubClient mqtt_;
  CommandCallback cmdCb_ = nullptr;
  bool lastConnected_ = false;

  uint32_t nextWifiRetryMs_ = 0;
  uint32_t nextMqttRetryMs_ = 0;

  static void onMqttMessage(char* topic, uint8_t* payload, unsigned int length);
  void connectWifi(uint32_t nowMs);
  void connectMqtt(uint32_t nowMs);
};
