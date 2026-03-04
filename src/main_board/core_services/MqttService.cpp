#include "core_services/MqttService.h"

#include <cstring>

#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/RuntimeStats.h"

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MQTT_BROKER
#define MQTT_BROKER "127.0.0.1"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "embedded-security-esp32"
#endif
#ifndef MQTT_TOPIC_CMD
#define MQTT_TOPIC_CMD "esh/main/cmd"
#endif
#ifndef MQTT_TOPIC_EVENT
#define MQTT_TOPIC_EVENT "esh/main/event"
#endif
#ifndef MQTT_TOPIC_STATUS
#define MQTT_TOPIC_STATUS "esh/main/status"
#endif
#ifndef MQTT_TOPIC_ACK
#define MQTT_TOPIC_ACK "esh/main/ack"
#endif
#ifndef MQTT_TOPIC_METRICS
#define MQTT_TOPIC_METRICS "esh/main/metrics"
#endif

#ifndef WIFI_RECONNECT_MS
#define WIFI_RECONNECT_MS 5000
#endif
#ifndef MQTT_RECONNECT_MS
#define MQTT_RECONNECT_MS 3000
#endif
#ifndef MQTT_KEEPALIVE_S
#define MQTT_KEEPALIVE_S 15
#endif
#ifndef MQTT_SOCKET_TIMEOUT_S
#define MQTT_SOCKET_TIMEOUT_S 1
#endif
#ifndef MQTT_METRICS_PERIOD_MS
#define MQTT_METRICS_PERIOD_MS 10000
#endif
#ifndef MQTT_PUB_DRAIN_BURST
#define MQTT_PUB_DRAIN_BURST 8
#endif

namespace {

bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

} // namespace

namespace CoreServices {

MqttService* MqttService::self_ = nullptr;

void MqttService::begin(QueueHandle_t commandQueue) {
  commandQueue_ = commandQueue;

  if (!publishQueue_) {
    publishQueue_ = xQueueCreate(32, sizeof(ConfigurationSharedTypes::PublishMessage));
    if (!publishQueue_) {
      Serial.println("[MQTT] publish queue init failed");
      delay(150);
      ESP.restart();
      return;
    }
  }

  self_ = this;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  mqtt_.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt_.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt_.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt_.setCallback(onMqttMessage_);

  const uint8_t ledPin = ConfigurationSharedTypes::Config::PIN_STATUS_LED;
  if (ledPin != ConfigurationSharedTypes::Config::PIN_UNUSED) {
    pinMode(ledPin, OUTPUT);
    setStatusLed_(false);
  }
}

void MqttService::loop(uint32_t nowMs) {
  connectWifi_(nowMs);
  connectMqtt_(nowMs);

  if (mqtt_.connected()) {
    mqtt_.loop();
    drainPublishQueue_();
  }
  updateConnectionSignal_(nowMs);

  if (reached(nowMs, nextMetricsMs_)) {
    nextMetricsMs_ = nowMs + MQTT_METRICS_PERIOD_MS;

    String payload = "{\"pub_drops\":";
    payload += String(publishDrops_);
    payload += ",\"cmd_drops\":";
    payload += String(commandDrops_);
    payload += ",\"event_drops\":";
    payload += String((uint32_t)ConfigurationSharedTypes::RuntimeStats::securityEventDrops);
    payload += ",\"q_pub\":";
    payload += String(publishQueue_ ? (uint32_t)uxQueueMessagesWaiting(publishQueue_) : 0u);
    payload += ",\"q_cmd\":";
    payload += String(commandQueue_ ? (uint32_t)uxQueueMessagesWaiting(commandQueue_) : 0u);
    payload += ",\"uptime_ms\":";
    payload += String(millis());
    payload += "}";

    mqtt_.publish(MQTT_TOPIC_METRICS, payload.c_str(), false);
  }
}

bool MqttService::enqueue(const ConfigurationSharedTypes::PublishMessage& msg) {
  if (!publishQueue_) return false;

  if (xQueueSend(publishQueue_, &msg, 0) != pdTRUE) {
    ++publishDrops_;
    return false;
  }

  return true;
}

bool MqttService::isConnected() {
  return mqtt_.connected();
}

void MqttService::setStatusLed_(bool connected) {
  const uint8_t ledPin = ConfigurationSharedTypes::Config::PIN_STATUS_LED;
  if (ledPin == ConfigurationSharedTypes::Config::PIN_UNUSED) return;

  const bool activeHigh = ConfigurationSharedTypes::Config::STATUS_LED_ACTIVE_HIGH;
  const uint8_t level = ((connected && activeHigh) || (!connected && !activeHigh)) ? HIGH : LOW;
  digitalWrite(ledPin, level);
}

void MqttService::updateConnectionSignal_(uint32_t nowMs) {
  const bool rawNow = mqtt_.connected();

  if (!hasRawConnected_) {
    rawConnected_ = rawNow;
    hasRawConnected_ = true;
    rawChangedAtMs_ = nowMs;
  } else if (rawNow != rawConnected_) {
    rawConnected_ = rawNow;
    rawChangedAtMs_ = nowMs;
  }

  if (!hasStableConnected_) {
    stableConnected_ = rawConnected_;
    hasStableConnected_ = true;
    setStatusLed_(stableConnected_);
    return;
  }

  if (stableConnected_ == rawConnected_) return;
  const uint32_t debounceMs = ConfigurationSharedTypes::Config::MQTT_CONN_DEBOUNCE_MS;
  if (!reached(nowMs, rawChangedAtMs_ + debounceMs)) return;

  stableConnected_ = rawConnected_;
  setStatusLed_(stableConnected_);
}

void MqttService::connectWifi_(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!reached(nowMs, nextWifiRetryMs_)) return;

  nextWifiRetryMs_ = nowMs + WIFI_RECONNECT_MS;
  if (strlen(WIFI_SSID) == 0) return;

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void MqttService::connectMqtt_(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt_.connected()) return;
  if (!reached(nowMs, nextMqttRetryMs_)) return;

  nextMqttRetryMs_ = nowMs + MQTT_RECONNECT_MS;

  const bool useAuth = strlen(MQTT_USERNAME) > 0;
  bool ok = false;

  if (useAuth) {
    ok = mqtt_.connect(MQTT_CLIENT_ID,
                       MQTT_USERNAME,
                       MQTT_PASSWORD,
                       MQTT_TOPIC_STATUS,
                       1,
                       true,
                       "{\"reason\":\"offline\"}");
  } else {
    ok = mqtt_.connect(MQTT_CLIENT_ID,
                       MQTT_TOPIC_STATUS,
                       1,
                       true,
                       "{\"reason\":\"offline\"}");
  }

  if (!ok) return;

  mqtt_.subscribe(MQTT_TOPIC_CMD);
  mqtt_.publish(MQTT_TOPIC_STATUS, "{\"reason\":\"online\"}", true);
}

void MqttService::drainPublishQueue_() {
  if (!publishQueue_) return;

  ConfigurationSharedTypes::PublishMessage msg{};
  uint32_t burst = 0;

  while (burst < MQTT_PUB_DRAIN_BURST && xQueueReceive(publishQueue_, &msg, 0) == pdTRUE) {
    bool ok = false;

    if (msg.kind == ConfigurationSharedTypes::PublishKind::event) {
      String payload = "{\"event\":\"";
      payload += ConfigurationSharedTypes::toString(msg.event.type);
      payload += "\",\"src\":";
      payload += String(msg.event.src);
      payload += ",\"flag\":\"";
      payload += msg.text1;
      payload += "\",\"mode\":\"";
      payload += ConfigurationSharedTypes::toString(msg.st.mode);
      payload += "\",\"level\":\"";
      payload += ConfigurationSharedTypes::toString(msg.st.level);
      payload += "\",\"failed_attempts\":";
      payload += String(msg.st.failed_attempts);
      payload += "\",\"ts_ms\":";
      payload += String(msg.event.ts_ms);
      payload += "}";
      ok = mqtt_.publish(MQTT_TOPIC_EVENT, payload.c_str(), false);
    } else if (msg.kind == ConfigurationSharedTypes::PublishKind::status) {
      String payload = "{\"reason\":\"";
      payload += msg.text1;
      payload += "\",\"mode\":\"";
      payload += ConfigurationSharedTypes::toString(msg.st.mode);
      payload += "\",\"level\":\"";
      payload += ConfigurationSharedTypes::toString(msg.st.level);
      payload += "\",\"latest_mode\":\"";
      payload += ConfigurationSharedTypes::toString(msg.st.latest_mode);
      payload += "\",\"is_night\":";
      payload += msg.st.is_night ? "true" : "false";
      payload += ",\"failed_attempts\":";
      payload += String(msg.st.failed_attempts);
      payload += ",\"door_locked\":";
      payload += msg.st.door_locked ? "true" : "false";
      payload += ",\"window_locked\":";
      payload += msg.st.window_locked ? "true" : "false";
      payload += ",\"door_open\":";
      payload += msg.st.door_open ? "true" : "false";
      payload += ",\"window_open\":";
      payload += msg.st.window_open ? "true" : "false";
      payload += ",\"event_drops\":";
      payload += String((uint32_t)ConfigurationSharedTypes::RuntimeStats::securityEventDrops);
      payload += ",\"uptime_ms\":";
      payload += String(millis());
      payload += "}";
      ok = mqtt_.publish(MQTT_TOPIC_STATUS, payload.c_str(), true);
    } else if (msg.kind == ConfigurationSharedTypes::PublishKind::ack) {
      String payload = "{\"cmd\":\"";
      payload += msg.text1;
      payload += "\",\"ok\":";
      payload += msg.ok ? "true" : "false";
      payload += ",\"detail\":\"";
      payload += msg.text2;
      payload += "\",\"uptime_ms\":";
      payload += String(millis());
      payload += "}";
      ok = mqtt_.publish(MQTT_TOPIC_ACK, payload.c_str(), false);
    }

    if (!ok) {
      ++publishDrops_;
    }

    ++burst;
  }
}

void MqttService::onMqttMessage_(char*, uint8_t* payload, unsigned int length) {
  if (!self_ || !self_->commandQueue_) return;

  ConfigurationSharedTypes::RemoteCommandMessage msg{};

  const unsigned int cap = (unsigned int)(sizeof(msg.payload) - 1);
  const unsigned int copyLen = (length < cap) ? length : cap;
  for (unsigned int i = 0; i < copyLen; ++i) {
    msg.payload[i] = (char)payload[i];
  }
  msg.payload[copyLen] = '\0';

  if (xQueueSend(self_->commandQueue_, &msg, 0) != pdTRUE) {
    ++self_->commandDrops_;
  }
}

} // namespace CoreServices
