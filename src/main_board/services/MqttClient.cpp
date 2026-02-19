#include "MqttClient.h"

#include <cstring>

MqttClient* MqttClient::self_ = nullptr;

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}
} // namespace

static const char* wlStatusText(wl_status_t st) {
  switch (st) {
    case WL_NO_SHIELD:       return "NO_SHIELD";
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

static const char* modeText(Mode mode) {
  switch (mode) {
    case Mode::startup_safe: return "startup_safe";
    case Mode::disarm: return "disarm";
    case Mode::away:   return "away";
    case Mode::night:  return "night";
    default:           return "unknown";
  }
}

static bool someoneHomeFromMode(Mode mode) {
  // Main board no longer runs local presence automation.
  // Publish a stable occupancy hint for the automation board.
  return mode != Mode::away;
}

static const char* levelText(AlarmLevel lv) {
  switch (lv) {
    case AlarmLevel::off:      return "off";
    case AlarmLevel::warn:     return "warn";
    case AlarmLevel::alert:    return "alert";
    case AlarmLevel::critical: return "critical";
    default:                   return "unknown";
  }
}

MqttClient::MqttClient()
: mqtt_(wifiClient_) {}

void MqttClient::begin(CommandCallback cb) {
  cmdCb_ = cb;
  self_ = this;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  mqtt_.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt_.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt_.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt_.setCallback(onMqttMessage);
}

void MqttClient::connectWifi(uint32_t nowMs) {
  const wl_status_t st = WiFi.status();
  if (st != lastWifiStatus_) {
    lastWifiStatus_ = st;
  }

  if (st == WL_CONNECTED) return;
  if (!reached(nowMs, nextWifiRetryMs_)) return;

  nextWifiRetryMs_ = nowMs + WIFI_RECONNECT_MS;

  if (strlen(WIFI_SSID) == 0) {
    return;
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void MqttClient::connectMqtt(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt_.connected()) return;
  if (!reached(nowMs, nextMqttRetryMs_)) return;

  nextMqttRetryMs_ = nowMs + MQTT_RECONNECT_MS;

  const bool hasAuth = strlen(MQTT_USERNAME) > 0;
  bool connected = false;

  if (hasAuth) {
    connected = mqtt_.connect(
      MQTT_CLIENT_ID,
      MQTT_USERNAME,
      MQTT_PASSWORD,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"reason\":\"offline\"}"
    );
  } else {
    connected = mqtt_.connect(
      MQTT_CLIENT_ID,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"reason\":\"offline\"}"
    );
  }

  if (!connected) {
    Serial.print("[MQTT] connect failed rc=");
    Serial.println(mqtt_.state());
    return;
  }

  mqtt_.subscribe(MQTT_TOPIC_CMD);

  lastConnected_ = true;
  mqtt_.publish(MQTT_TOPIC_STATUS, "{\"reason\":\"online\"}", false);
}

void MqttClient::update(uint32_t nowMs) {
  if (lastConnected_ && !mqtt_.connected()) {
    lastConnected_ = false;
    Serial.println("[MQTT] disconnected");
  }

  connectWifi(nowMs);
  connectMqtt(nowMs);

  if (mqtt_.connected()) {
    mqtt_.loop();
  }
}

bool MqttClient::ready() {
  return mqtt_.connected();
}

bool MqttClient::publishEvent(const Event& e, const SystemState& st, const Command& cmd) {
  if (!ready()) return false;

  String payload = "{\"event\":\"";
  payload += toString(e.type);
  payload += "\",\"src\":";
  payload += String(e.src);
  payload += ",\"cmd\":\"";
  payload += toString(cmd.type);
  payload += "\",\"mode\":\"";
  payload += modeText(st.mode);
  payload += "\",\"isSomeoneHome\":";
  payload += someoneHomeFromMode(st.mode) ? "true" : "false";
  payload += ",\"level\":\"";
  payload += levelText(st.level);
  payload += "\",\"door_locked\":";
  payload += st.door_locked ? "true" : "false";
  payload += ",\"window_locked\":";
  payload += st.window_locked ? "true" : "false";
  payload += ",\"door_open\":";
  payload += st.door_open ? "true" : "false";
  payload += ",\"window_open\":";
  payload += st.window_open ? "true" : "false";
  payload += ",\"ts_ms\":";
  payload += String(e.ts_ms);
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_EVENT, payload.c_str(), true);
}

bool MqttClient::publishStatus(const SystemState& st, const char* reason) {
  if (!ready()) return false;

  String payload = "{\"reason\":\"";
  payload += (reason ? reason : "unknown");
  payload += "\",\"mode\":\"";
  payload += modeText(st.mode);
  payload += "\",\"isSomeoneHome\":";
  payload += someoneHomeFromMode(st.mode) ? "true" : "false";
  payload += ",\"level\":\"";
  payload += levelText(st.level);
  payload += "\",\"door_locked\":";
  payload += st.door_locked ? "true" : "false";
  payload += ",\"window_locked\":";
  payload += st.window_locked ? "true" : "false";
  payload += ",\"door_open\":";
  payload += st.door_open ? "true" : "false";
  payload += ",\"window_open\":";
  payload += st.window_open ? "true" : "false";
  payload += ",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_STATUS, payload.c_str(), true);
}

bool MqttClient::publishAck(const char* cmd, bool ok, const char* detail) {
  if (!ready()) return false;

  String payload = "{\"cmd\":\"";
  payload += (cmd ? cmd : "");
  payload += "\",\"ok\":";
  payload += ok ? "true" : "false";
  payload += ",\"detail\":\"";
  payload += (detail ? detail : "");
  payload += "\",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_ACK, payload.c_str(), false);
}

bool MqttClient::publishMetrics(
  uint32_t usDrops,
  uint32_t pubDrops,
  uint32_t cmdDrops,
  uint32_t storeDrops,
  uint32_t usQueueDepth,
  uint32_t pubQueueDepth,
  uint32_t cmdQueueDepth,
  uint32_t storeDepth
) {
  if (!ready()) return false;

  String payload = "{\"us_drops\":";
  payload += String(usDrops);
  payload += ",\"pub_drops\":";
  payload += String(pubDrops);
  payload += ",\"cmd_drops\":";
  payload += String(cmdDrops);
  payload += ",\"store_drops\":";
  payload += String(storeDrops);
  payload += ",\"q_us\":";
  payload += String(usQueueDepth);
  payload += ",\"q_pub\":";
  payload += String(pubQueueDepth);
  payload += ",\"q_cmd\":";
  payload += String(cmdQueueDepth);
  payload += ",\"q_store\":";
  payload += String(storeDepth);
  payload += ",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_METRICS, payload.c_str(), false);
}

void MqttClient::onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (!self_ || !self_->cmdCb_) return;

  String t = topic ? String(topic) : String("");
  String p;
  p.reserve(length);
  for (unsigned int i = 0; i < length; ++i) {
    p += (char)payload[i];
  }

  self_->cmdCb_(t, p);
}
