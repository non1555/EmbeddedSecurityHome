#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "configuration_shared_types/Types.h"

namespace CoreServices {

class MqttService {
public:
  void begin(QueueHandle_t commandQueue); // Initializes WiFi/MQTT clients and binds command queue. Params: commandQueue=RTOS queue receiving remote commands.
  void loop(uint32_t nowMs); // Runs non-blocking reconnect, MQTT polling, and publish draining. Params: nowMs=current timestamp in ms.

  bool enqueue(const ConfigurationSharedTypes::PublishMessage& msg); // Queues outbound MQTT payload for async publish. Params: msg=message to publish.
  bool isConnected(); // Returns whether MQTT session is currently connected. Params: none.

private:
  static MqttService* self_;

  WiFiClient wifi_;
  PubSubClient mqtt_{wifi_};

  QueueHandle_t commandQueue_ = nullptr;
  QueueHandle_t publishQueue_ = nullptr;

  uint32_t nextWifiRetryMs_ = 0;
  uint32_t nextMqttRetryMs_ = 0;
  uint32_t nextMetricsMs_ = 0;
  uint32_t publishDrops_ = 0;
  uint32_t commandDrops_ = 0;

  void connectWifi_(uint32_t nowMs); // Attempts non-blocking WiFi reconnect on retry schedule. Params: nowMs=current timestamp in ms.
  void connectMqtt_(uint32_t nowMs); // Attempts non-blocking MQTT reconnect when WiFi is ready. Params: nowMs=current timestamp in ms.
  void drainPublishQueue_(); // Flushes buffered publish messages to MQTT broker. Params: none.

  static void onMqttMessage_(char* topic, uint8_t* payload, unsigned int length); // MQTT callback that forwards payload into command queue. Params: topic=received topic name, payload=raw payload bytes, length=payload byte length.
};

} // namespace CoreServices
