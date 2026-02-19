#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "actuators/OutputActuator.h"
#include "automation/light_system.h"
#include "automation/presence.h"
#include "automation/temp_system.h"
#include "drivers/NetworkDriver.h"
#include "hardware/AutoHardwareConfig.h"
#include "pipelines/AutomationPipeline.h"
#include "rtos/TaskRunner.h"
#include "sensors/ClimateSensor.h"
#include "sensors/LightSensor.h"
#include "app/AutomationRuntime.h"

#ifndef FW_CMD_TOKEN
#define FW_CMD_TOKEN ""
#endif

#ifndef MQTT_TOPIC_MAIN_STATUS
#define MQTT_TOPIC_MAIN_STATUS "esh/main/status"
#endif

#ifndef MQTT_TOPIC_ACK
#define MQTT_TOPIC_ACK "esh/auto/ack"
#endif

#ifndef MAIN_CONTEXT_STALE_MS
#define MAIN_CONTEXT_STALE_MS 30000
#endif

namespace {

String normalize(String s) {
  s.trim();
  s.toLowerCase();
  return s;
}

constexpr uint32_t REMOTE_NONCE_TTL_MS = 180000;
constexpr uint32_t MAIN_CONTEXT_MAX_AGE_MS = MAIN_CONTEXT_STALE_MS;
constexpr size_t NONCE_SLOTS = 24;

struct NonceSlot {
  bool used = false;
  uint32_t hash = 0;
  uint32_t expiresAtMs = 0;
};

NonceSlot nonceSlots[NONCE_SLOTS];
size_t nonceCursor = 0;
Preferences noncePref;
bool nonceCounterReady = false;
uint32_t lastRemoteNonce = 0;

bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

uint32_t fnv1a(const String& s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); ++i) {
    h ^= static_cast<uint8_t>(s[i]);
    h *= 16777619u;
  }
  return h;
}

bool parseUint32Strict(const String& s, uint32_t& out) {
  if (s.length() == 0) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c < '0' || c > '9') return false;
    v = (v * 10u) + (uint64_t)(c - '0');
    if (v > 0xFFFFFFFFull) return false;
  }
  out = (uint32_t)v;
  return true;
}

bool acceptNonce(const String& nonce, uint32_t nowMs, uint32_t ttlMs) {
  if (nonce.length() == 0 || ttlMs == 0) return false;
  const uint32_t h = fnv1a(nonce);

  for (size_t i = 0; i < NONCE_SLOTS; ++i) {
    if (!nonceSlots[i].used) continue;
    if (reached(nowMs, nonceSlots[i].expiresAtMs)) continue;
    if (nonceSlots[i].hash == h) return false;
  }

  nonceSlots[nonceCursor].used = true;
  nonceSlots[nonceCursor].hash = h;
  nonceSlots[nonceCursor].expiresAtMs = nowMs + ttlMs;
  nonceCursor = (nonceCursor + 1u) % NONCE_SLOTS;
  return true;
}

bool parseAuthorizedCommand(const String& payload, String& outCmd) {
  const String configuredToken = normalize(String(FW_CMD_TOKEN));
  if (configuredToken.length() == 0) {
    outCmd = normalize(payload);
    return outCmd == "status";
  }

  const int firstSep = payload.indexOf('|');
  if (firstSep <= 0) return false;

  String presentedToken = normalize(payload.substring(0, firstSep));
  if (presentedToken != configuredToken) return false;

  const int secondSep = payload.indexOf('|', firstSep + 1);
  if (secondSep < 0) return false;

  String noncePart = normalize(payload.substring(firstSep + 1, secondSep));
  String commandPart = normalize(payload.substring(secondSep + 1));
  if (noncePart.length() == 0 || commandPart.length() == 0) return false;
  const bool readOnlyStatus = (commandPart == "status");
  if (!nonceCounterReady && !readOnlyStatus) return false;

  uint32_t parsed = 0;
  if (!parseUint32Strict(noncePart, parsed)) return false;
  if (parsed <= lastRemoteNonce) return false;
  if (!acceptNonce(noncePart, millis(), REMOTE_NONCE_TTL_MS)) return false;

  lastRemoteNonce = parsed;
  if (nonceCounterReady && !readOnlyStatus) {
    noncePref.putULong("rnonce", lastRemoteNonce);
  }

  outCmd = commandPart;
  return true;
}

enum class MainMode : uint8_t {
  unknown = 0,
  startup_safe,
  disarm,
  away,
  night,
};

MainMode parseMainMode(const String& mode) {
  const String m = normalize(mode);
  if (m == "startup_safe") return MainMode::startup_safe;
  if (m == "disarm") return MainMode::disarm;
  if (m == "away") return MainMode::away;
  if (m == "night") return MainMode::night;
  return MainMode::unknown;
}

const char* toText(MainMode mode) {
  switch (mode) {
    case MainMode::startup_safe: return "startup_safe";
    case MainMode::disarm: return "disarm";
    case MainMode::away: return "away";
    case MainMode::night: return "night";
    case MainMode::unknown:
    default: return "unknown";
  }
}

inline bool isJsonWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

inline bool isJsonDelimiter(char c) {
  return c == ',' || c == '}' || c == ']' || isJsonWhitespace(c);
}

int findJsonValueStart(const String& payload, const char* key) {
  if (!key || *key == '\0') return -1;
  const String needle = String("\"") + String(key) + String("\"");
  int from = 0;
  while (from < (int)payload.length()) {
    const int keyPos = payload.indexOf(needle, from);
    if (keyPos < 0) return -1;

    int pos = keyPos + (int)needle.length();
    while (pos < (int)payload.length() && isJsonWhitespace(payload[pos])) ++pos;
    if (pos >= (int)payload.length() || payload[pos] != ':') {
      from = keyPos + 1;
      continue;
    }
    ++pos;
    while (pos < (int)payload.length() && isJsonWhitespace(payload[pos])) ++pos;
    if (pos >= (int)payload.length()) return -1;
    return pos;
  }
  return -1;
}

bool extractJsonStringField(const String& payload, const char* key, String& out) {
  const int valueStart = findJsonValueStart(payload, key);
  if (valueStart < 0 || payload[valueStart] != '"') return false;

  out = "";
  bool escaping = false;
  for (int i = valueStart + 1; i < (int)payload.length(); ++i) {
    const char c = payload[i];
    if (escaping) {
      out += c;
      escaping = false;
      continue;
    }
    if (c == '\\') {
      escaping = true;
      continue;
    }
    if (c == '"') {
      return true;
    }
    out += c;
  }
  return false;
}

bool extractJsonBoolField(const String& payload, const char* key, bool& out) {
  const int valueStart = findJsonValueStart(payload, key);
  if (valueStart < 0) return false;

  if (payload.startsWith("true", valueStart)) {
    const int end = valueStart + 4;
    if (end >= (int)payload.length() || isJsonDelimiter(payload[end])) {
      out = true;
      return true;
    }
  }
  if (payload.startsWith("false", valueStart)) {
    const int end = valueStart + 5;
    if (end >= (int)payload.length() || isJsonDelimiter(payload[end])) {
      out = false;
      return true;
    }
  }
  return false;
}

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Shared state (access guarded by stateMu).
SemaphoreHandle_t stateMu = nullptr;
bool lightOn = false;
bool fanOn = false;
bool lightAuto = true;
bool fanAuto = true;
float lastLux = NAN;
bool lastLuxOk = false;
float lastTempC = NAN;
float lastHum = NAN;
bool hasMainMode = false;
MainMode lastMainMode = MainMode::unknown;
uint32_t lastMainModeMs = 0;
bool hasMainPresence = false;
uint32_t lastMainPresenceMs = 0;

uint32_t nextStatusMs = 0;
constexpr uint32_t STATUS_PERIOD_MS = 5000;

uint32_t nextWifiRetryMs = 0;
uint32_t nextMqttRetryMs = 0;
constexpr uint32_t WIFI_RETRY_MS = 5000;
constexpr uint32_t MQTT_RETRY_MS = 3000;

TaskHandle_t taskControlHandle = nullptr;
TaskHandle_t taskNetHandle = nullptr;

uint32_t nextLightLogMs = 0;
constexpr uint32_t LIGHT_LOG_MS = 1000;
uint32_t nextClimateLogMs = 0;
constexpr uint32_t CLIMATE_LOG_MS = 1000;
constexpr TickType_t STATE_LOCK_TIMEOUT_TICKS = pdMS_TO_TICKS(20);
constexpr uint32_t STATE_LOCK_WARN_COOLDOWN_MS = 5000;
uint32_t nextStateLockWarnMs = 0;

inline bool dueOrUnset(uint32_t nowMs, uint32_t targetMs) {
  return targetMs == 0 || reached(nowMs, targetMs);
}

bool contextFresh(bool hasContext, uint32_t lastUpdateMs, uint32_t nowMs) {
  if (!hasContext) return false;
  if (MAIN_CONTEXT_MAX_AGE_MS == 0) return true;
  return !reached(nowMs, lastUpdateMs + MAIN_CONTEXT_MAX_AGE_MS);
}

bool tryLockState(uint32_t nowMs, const char* where) {
  if (!stateMu) return true;
  if (xSemaphoreTake(stateMu, STATE_LOCK_TIMEOUT_TICKS) == pdTRUE) return true;

  if (reached(nowMs, nextStateLockWarnMs)) {
    nextStateLockWarnMs = nowMs + STATE_LOCK_WARN_COOLDOWN_MS;
    Serial.print("[auto] WARN: state mutex timeout @");
    Serial.println(where ? where : "unknown");
  }
  return false;
}

void unlockState() {
  if (stateMu) xSemaphoreGive(stateMu);
}

void applyOutputs() {
  bool lightOnCopy = false;
  bool fanOnCopy = false;
  const uint32_t nowMs = millis();
  if (!tryLockState(nowMs, "applyOutputs")) return;
  lightOnCopy = lightOn;
  fanOnCopy = fanOn;
  unlockState();
  OutputActuator::apply({lightOnCopy, fanOnCopy});
}

void publishStatus(const char* reason) {
  bool lightOnCopy = false;
  bool fanOnCopy = false;
  bool lightAutoCopy = true;
  bool fanAutoCopy = true;
  bool luxOkCopy = false;
  float luxCopy = NAN;
  float tCopy = NAN;
  float hCopy = NAN;
  bool hasMainModeCopy = false;
  MainMode mainModeCopy = MainMode::unknown;
  uint32_t mainModeMsCopy = 0;
  bool mainModeFreshCopy = false;
  bool hasMainPresenceCopy = false;
  bool someoneHomeCopy = true;
  uint32_t mainPresenceMsCopy = 0;
  bool mainPresenceFreshCopy = false;
  const uint32_t nowMs = millis();

  if (!tryLockState(nowMs, "publishStatus")) return;
  lightOnCopy = lightOn;
  fanOnCopy = fanOn;
  lightAutoCopy = lightAuto;
  fanAutoCopy = fanAuto;
  luxOkCopy = lastLuxOk;
  luxCopy = lastLux;
  tCopy = lastTempC;
  hCopy = lastHum;
  hasMainModeCopy = hasMainMode;
  mainModeCopy = lastMainMode;
  mainModeMsCopy = lastMainModeMs;
  hasMainPresenceCopy = hasMainPresence;
  someoneHomeCopy = isSomeoneHome;
  mainPresenceMsCopy = lastMainPresenceMs;
  unlockState();
  mainModeFreshCopy = contextFresh(hasMainModeCopy, mainModeMsCopy, nowMs);
  mainPresenceFreshCopy = contextFresh(hasMainPresenceCopy, mainPresenceMsCopy, nowMs);

  String payload = "{\"node\":\"auto\",\"reason\":\"";
  payload += (reason ? reason : "unknown");
  payload += "\",\"led\":";
  payload += lightOnCopy ? "true" : "false";
  payload += ",\"light\":";
  payload += lightOnCopy ? "true" : "false";
  payload += ",\"light_auto\":";
  payload += lightAutoCopy ? "true" : "false";
  payload += ",\"fan\":";
  payload += fanOnCopy ? "true" : "false";
  payload += ",\"fan_auto\":";
  payload += fanAutoCopy ? "true" : "false";
  if (luxOkCopy && !isnan(luxCopy)) {
    payload += ",\"lux\":";
    payload += String(luxCopy, 1);
  }
  if (!isnan(tCopy)) {
    payload += ",\"temp_c\":";
    payload += String(tCopy, 1);
  }
  if (!isnan(hCopy)) {
    payload += ",\"hum\":";
    payload += String(hCopy, 1);
  }
  if (hasMainModeCopy) {
    payload += ",\"main_mode\":\"";
    payload += toText(mainModeCopy);
    payload += "\",\"main_mode_age_ms\":";
    payload += String(nowMs - mainModeMsCopy);
    payload += ",\"main_mode_stale\":";
    payload += mainModeFreshCopy ? "false" : "true";
  }
  if (hasMainPresenceCopy) {
    payload += ",\"main_is_someone_home\":";
    payload += someoneHomeCopy ? "true" : "false";
    payload += ",\"main_is_someone_home_age_ms\":";
    payload += String(nowMs - mainPresenceMsCopy);
    payload += ",\"main_is_someone_home_stale\":";
    payload += mainPresenceFreshCopy ? "false" : "true";
  }
  payload += ",\"uptime_ms\":";
  payload += String(nowMs);
  payload += "}";

  if (mqtt.connected()) {
    mqtt.publish(MQTT_TOPIC_STATUS, payload.c_str(), true);
  }
}

void publishAck(const char* cmd, bool ok, const char* detail) {
  if (!mqtt.connected()) return;

  String payload = "{\"cmd\":\"";
  payload += (cmd ? cmd : "");
  payload += "\",\"ok\":";
  payload += ok ? "true" : "false";
  payload += ",\"detail\":\"";
  payload += (detail ? detail : "");
  payload += "\",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";
  mqtt.publish(MQTT_TOPIC_ACK, payload.c_str(), false);
}

void logLight(uint32_t nowMs) {
  if (nextLightLogMs != 0 && !reached(nowMs, nextLightLogMs)) return;
  nextLightLogMs = nowMs + LIGHT_LOG_MS;

  bool lightOnCopy = false;
  bool lightAutoCopy = true;
  bool luxOkCopy = false;
  float luxCopy = NAN;

  if (!tryLockState(nowMs, "logLight")) return;
  lightOnCopy = lightOn;
  lightAutoCopy = lightAuto;
  luxOkCopy = lastLuxOk;
  luxCopy = lastLux;
  unlockState();

  Serial.print("[light] auto=");
  Serial.print(lightAutoCopy ? "1" : "0");
  Serial.print(" led=");
  Serial.print(lightOnCopy ? "ON" : "OFF");
  Serial.print(" lux=");
  if (luxOkCopy) Serial.print(luxCopy, 1);
  else Serial.print("ERR");
  Serial.println();
}

void logClimate(uint32_t nowMs) {
  if (nextClimateLogMs != 0 && !reached(nowMs, nextClimateLogMs)) return;
  nextClimateLogMs = nowMs + CLIMATE_LOG_MS;

  bool fanOnCopy = false;
  bool fanAutoCopy = true;
  float tCopy = NAN;
  float hCopy = NAN;

  if (!tryLockState(nowMs, "logClimate")) return;
  fanOnCopy = fanOn;
  fanAutoCopy = fanAuto;
  tCopy = lastTempC;
  hCopy = lastHum;
  unlockState();

  Serial.print("[climate] auto=");
  Serial.print(fanAutoCopy ? "1" : "0");
  Serial.print(" fan=");
  Serial.print(fanOnCopy ? "ON" : "OFF");
  Serial.print(" temp=");
  if (!ClimateSensor::available()) Serial.print("NA");
  else if (!isnan(tCopy)) Serial.print(tCopy, 1);
  else Serial.print("ERR");
  Serial.print(" hum=");
  if (!ClimateSensor::available()) Serial.print("NA");
  else if (!isnan(hCopy)) Serial.print(hCopy, 1);
  else Serial.print("ERR");
  Serial.println();
}

void logNetIfChanged() {
  static wl_status_t lastWifi = WL_IDLE_STATUS;
  static bool lastMqtt = false;
  static int lastRc = 0;
  static uint32_t nextPeriodicMs = 0;

  const wl_status_t wifi = WiFi.status();
  const bool mc = mqtt.connected();
  const int rc = mc ? 0 : mqtt.state();

  const uint32_t now = millis();
  const bool changed = !(wifi == lastWifi && mc == lastMqtt && rc == lastRc);
  const bool periodic = dueOrUnset(now, nextPeriodicMs);
  if (!changed && !periodic) return;

  nextPeriodicMs = now + 1000;
  lastWifi = wifi;
  lastMqtt = mc;
  lastRc = rc;

  Serial.print("[net] wifi=");
  Serial.print(wifi == WL_CONNECTED ? "1" : "0");
  Serial.print(" mqtt=");
  Serial.print(mc ? "1" : "0");
  if (!mc) {
    Serial.print(" rc=");
    Serial.print(rc);
  }
  Serial.println();
}

void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  const String topicStr = topic ? String(topic) : String("");
  String raw;
  raw.reserve(length);
  for (unsigned int i = 0; i < length; ++i) raw += static_cast<char>(payload[i]);

  if (topicStr == String(MQTT_TOPIC_MAIN_STATUS)) {
    String modeText;
    const bool hasModeField = extractJsonStringField(raw, "mode", modeText);
    const MainMode parsedMode = hasModeField ? parseMainMode(modeText) : MainMode::unknown;

    bool someoneHome = false;
    const bool hasPresenceField =
      extractJsonBoolField(raw, "isSomeoneHome", someoneHome) ||
      extractJsonBoolField(raw, "someone_home", someoneHome);

    const bool hasValidMode = hasModeField && parsedMode != MainMode::unknown;
    if (!hasValidMode && !hasPresenceField) return;

    const uint32_t nowMs = millis();
    if (tryLockState(nowMs, "main status context")) {
      if (hasValidMode) {
        hasMainMode = true;
        lastMainMode = parsedMode;
        lastMainModeMs = nowMs;
      }
      if (hasPresenceField) {
        hasMainPresence = true;
        Presence::setExternalHome(someoneHome, nowMs);
        lastMainPresenceMs = nowMs;
      }
      unlockState();
    }
    return;
  }

  if (topicStr != String(MQTT_TOPIC_CMD)) return;

  String cmd;
  if (!parseAuthorizedCommand(raw, cmd)) {
    publishAck("auth", false, "unauthorized");
    publishStatus("auth_reject");
    return;
  }

  if (cmd == "light auto") {
    if (!tryLockState(millis(), "cmd light auto")) {
      publishAck("light auto", false, "state busy");
      publishStatus("state_busy");
      return;
    }
    lightAuto = true;
    unlockState();
    publishAck("light auto", true, "ok");
    publishStatus("light_auto");
  } else if (cmd == "light on") {
    if (!tryLockState(millis(), "cmd light on")) {
      publishAck("light on", false, "state busy");
      publishStatus("state_busy");
      return;
    }
    lightAuto = false;
    lightOn = true;
    unlockState();
    applyOutputs();
    publishAck("light on", true, "ok");
    publishStatus("light_on");
  } else if (cmd == "light off") {
    if (!tryLockState(millis(), "cmd light off")) {
      publishAck("light off", false, "state busy");
      publishStatus("state_busy");
      return;
    }
    lightAuto = false;
    lightOn = false;
    unlockState();
    applyOutputs();
    publishAck("light off", true, "ok");
    publishStatus("light_off");
  } else if (cmd == "fan on") {
    if (!tryLockState(millis(), "cmd fan on")) {
      publishAck("fan on", false, "state busy");
      publishStatus("state_busy");
      return;
    }
    fanAuto = false;
    fanOn = true;
    unlockState();
    applyOutputs();
    publishAck("fan on", true, "ok");
    publishStatus("fan_on");
  } else if (cmd == "fan off") {
    if (!tryLockState(millis(), "cmd fan off")) {
      publishAck("fan off", false, "state busy");
      publishStatus("state_busy");
      return;
    }
    fanAuto = false;
    fanOn = false;
    unlockState();
    applyOutputs();
    publishAck("fan off", true, "ok");
    publishStatus("fan_off");
  } else if (cmd == "fan auto") {
    if (!tryLockState(millis(), "cmd fan auto")) {
      publishAck("fan auto", false, "state busy");
      publishStatus("state_busy");
      return;
    }
    fanAuto = true;
    unlockState();
    publishAck("fan auto", true, "ok");
    publishStatus("fan_auto");
  } else if (cmd == "status") {
    publishAck("status", true, "ok");
    publishStatus("status");
  } else {
    publishAck("unknown", false, "unsupported command");
    publishStatus("unsupported_cmd");
  }
}

void connectWifi(uint32_t nowMs) {
  NetworkDriver::tryConnectWifi(nowMs, nextWifiRetryMs, WIFI_RETRY_MS);
}

void connectMqtt(uint32_t nowMs) {
  const bool connected = NetworkDriver::tryConnectMqtt(
    mqtt,
    nowMs,
    nextMqttRetryMs,
    MQTT_RETRY_MS
  );
  if (!connected) return;
  mqtt.subscribe(MQTT_TOPIC_CMD);
  if (String(MQTT_TOPIC_MAIN_STATUS) != String(MQTT_TOPIC_CMD)) {
    mqtt.subscribe(MQTT_TOPIC_MAIN_STATUS);
  }
  publishStatus("online");
}

void taskControl(void*) {
  uint32_t nextLuxMs = 0;
  uint32_t nextDhtMs = 0;

  for (;;) {
    const uint32_t now = millis();
    if (tryLockState(now, "presence tick")) {
      Presence::tick(now);
      unlockState();
    }

    // Read lux and run local light automation.
    if (LightSensor::isReady() && dueOrUnset(now, nextLuxMs)) {
      nextLuxMs = now + AutoHw::LIGHT_SAMPLE_MS;

      float lux = NAN;
      const bool ok = LightSensor::readLux(lux);

      bool doAuto = true;
      bool curLight = false;
      bool allowByMainMode = true;
      bool allowByMainPresence = true;
      if (tryLockState(now, "taskControl lux")) {
        lastLuxOk = ok;
        lastLux = lux;
        doAuto = lightAuto;
        curLight = lightOn;
        // Policy:
        // - If main context exists, keep using the latest known values even when stale.
        // - If no context has ever been received, use conservative fallback (keep auto-light off).
        allowByMainMode = hasMainMode && lastMainMode != MainMode::away;
        allowByMainPresence = hasMainPresence && isSomeoneHome;
        unlockState();
      } else {
        doAuto = false;
      }

      const bool newLight = AutomationPipeline::nextLight(
        doAuto,
        curLight,
        ok,
        lux,
        allowByMainMode,
        allowByMainPresence
      );
      if (newLight != curLight) {
        if (tryLockState(now, "taskControl set light")) {
          lightOn = newLight;
          unlockState();
          applyOutputs();
        }
      }
    }

    // Read DHT at a slower cadence.
    if (ClimateSensor::available() && dueOrUnset(now, nextDhtMs)) {
      nextDhtMs = now + AutoHw::TEMP_SAMPLE_MS;
      float t = NAN;
      float h = NAN;
      ClimateSensor::read(t, h);

      bool doFanAuto = false;
      bool curFan = false;
      bool allowByMainMode = true;
      bool allowByMainPresence = true;
      if (tryLockState(now, "taskControl dht")) {
        lastTempC = t;
        lastHum = h;
        doFanAuto = fanAuto;
        curFan = fanOn;
        // Policy:
        // - If main context exists, keep using the latest known values even when stale.
        // - If no context has ever been received, use conservative fallback (keep auto-fan off).
        allowByMainMode = hasMainMode && lastMainMode != MainMode::away;
        allowByMainPresence = hasMainPresence && isSomeoneHome;
        unlockState();
      }

      const bool newFan = AutomationPipeline::nextFan(
        doFanAuto,
        curFan,
        t,
        allowByMainMode,
        allowByMainPresence
      );
      if (newFan != curFan) {
        if (tryLockState(now, "taskControl set fan")) {
          fanOn = newFan;
          unlockState();
          applyOutputs();
        }
      }
    }

    logLight(now);
    logClimate(now);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void taskNet(void*) {
  for (;;) {
    const uint32_t now = millis();

    connectWifi(now);
    connectMqtt(now);
    logNetIfChanged();

    if (mqtt.connected()) {
      mqtt.loop();
    }

    if (dueOrUnset(now, nextStatusMs)) {
      nextStatusMs = now + STATUS_PERIOD_MS;
      publishStatus("periodic");
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

} // namespace

namespace AutoRuntime {

void begin() {
  Presence::init();
  LightSystem::init();
  TempSystem::init();
  OutputActuator::init();

  stateMu = xSemaphoreCreateMutex();
  if (!stateMu) {
    Serial.println("[auto] FATAL: state mutex create failed; halting");
    while (true) {
      delay(1000);
    }
  }

  NetworkDriver::initWifiSta();
  NetworkDriver::initMqtt(mqtt, onMqttMessage);

  nonceCounterReady = noncePref.begin("eshautov2", false);
  if (nonceCounterReady) {
    lastRemoteNonce = noncePref.getULong("rnonce", 0);
  } else {
    lastRemoteNonce = 0;
    Serial.println("[auto] WARN: nonce persistence unavailable; mutating remote commands blocked");
  }

  LightSensor::begin();
  if (LightSensor::isReady()) {
    Serial.print("[auto] BH1750 OK addr=0x");
    Serial.println(LightSensor::address(), HEX);
  } else {
    Serial.print("[auto] BH1750 not found (addr 0x");
    Serial.print(AutoHw::BH1750_ADDR_PRIMARY, HEX);
    Serial.print("/0x");
    Serial.print(AutoHw::BH1750_ADDR_SECONDARY, HEX);
    Serial.println(")");
  }

  applyOutputs();
  ClimateSensor::begin();
  if (ClimateSensor::available()) {
    Serial.println("[auto] DHT ready");
  } else {
    Serial.println("[auto] DHT disabled (PIN_UNUSED)");
  }

  const uint32_t now = millis();
  connectWifi(now);
  connectMqtt(now);
  publishStatus("boot");

  TaskRunner::start(taskControl, taskNet, &taskControlHandle, &taskNetHandle);
}

void tick(uint32_t nowMs) {
  (void)nowMs;
  // Everything runs in FreeRTOS tasks.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

} // namespace AutoRuntime
