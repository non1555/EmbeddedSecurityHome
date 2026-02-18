#include "drivers/NetworkDriver.h"

#include <WiFi.h>

namespace NetworkDriver {

void initWifiSta() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
}

void initMqtt(PubSubClient& mqtt, void (*callback)(char*, uint8_t*, unsigned int)) {
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt.setCallback(callback);
}

void tryConnectWifi(uint32_t nowMs, uint32_t& nextRetryMs, uint32_t retryMs) {
  if (WiFi.status() == WL_CONNECTED) return;
  if ((int32_t)(nowMs - nextRetryMs) < 0) return;
  nextRetryMs = nowMs + retryMs;
  if (strlen(WIFI_SSID) == 0) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool tryConnectMqtt(PubSubClient& mqtt, uint32_t nowMs, uint32_t& nextRetryMs, uint32_t retryMs) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqtt.connected()) return false;
  if ((int32_t)(nowMs - nextRetryMs) < 0) return false;
  nextRetryMs = nowMs + retryMs;

  const bool hasAuth = strlen(MQTT_USERNAME) > 0;
  if (hasAuth) {
    return mqtt.connect(
      MQTT_CLIENT_ID,
      MQTT_USERNAME,
      MQTT_PASSWORD,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"node\":\"auto\",\"reason\":\"offline\"}"
    );
  }

  return mqtt.connect(
    MQTT_CLIENT_ID,
    MQTT_TOPIC_STATUS,
    1,
    true,
    "{\"node\":\"auto\",\"reason\":\"offline\"}"
  );
}

} // namespace NetworkDriver
