#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>

#include "fw_auto/AutoHardwareConfig.h"

#define DHTTYPE DHT11

namespace {

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DHT dht(AutoHw::PIN_DHT, DHTTYPE);

// BH1750 (optional)
uint8_t bhAddr = 0;
bool bhReady = false;
constexpr uint8_t BH_ADDR_1 = 0x23;
constexpr uint8_t BH_ADDR_2 = 0x5C;
constexpr uint8_t BH_CMD_POWER_ON = 0x01;
constexpr uint8_t BH_CMD_RESET = 0x07;
constexpr uint8_t BH_CMD_CONT_HIRES = 0x10;

// Shared state (access guarded by stateMu).
SemaphoreHandle_t stateMu = nullptr;
bool lightOn = false;
bool fanOn = false;
bool lightAuto = true;
float lastLux = NAN;
bool lastLuxOk = false;
float lastTempC = NAN;
float lastHum = NAN;

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

bool bhWrite(uint8_t addr, uint8_t cmd) {
  Wire.beginTransmission(addr);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

bool bhInitAt(uint8_t addr) {
  if (!bhWrite(addr, BH_CMD_POWER_ON)) return false;
  delay(10);
  if (!bhWrite(addr, BH_CMD_RESET)) return false;
  delay(10);
  if (!bhWrite(addr, BH_CMD_CONT_HIRES)) return false;
  delay(180);
  return true;
}

bool bhReadLux(float& luxOut) {
  if (!bhReady || bhAddr == 0) return false;
  Wire.requestFrom((int)bhAddr, 2);
  if (Wire.available() < 2) return false;
  const uint16_t raw = (uint16_t(Wire.read()) << 8) | uint16_t(Wire.read());
  luxOut = raw / 1.2f;
  return true;
}

void applyOutputs() {
  if (AutoHw::PIN_RELAY_LIGHT != AutoHw::PIN_UNUSED) {
    pinMode(AutoHw::PIN_RELAY_LIGHT, OUTPUT);
    digitalWrite(AutoHw::PIN_RELAY_LIGHT, lightOn ? HIGH : LOW);
  }

  // Direct on/off only (no PWM). If you need PWM, use ledc* like temp_system.cpp.
  if (AutoHw::PIN_FAN_SWITCH != AutoHw::PIN_UNUSED) {
    pinMode(AutoHw::PIN_FAN_SWITCH, OUTPUT);
    digitalWrite(AutoHw::PIN_FAN_SWITCH, fanOn ? HIGH : LOW);
  }

  if (AutoHw::PIN_STATUS_LED != AutoHw::PIN_UNUSED) {
    pinMode(AutoHw::PIN_STATUS_LED, OUTPUT);
    digitalWrite(AutoHw::PIN_STATUS_LED, (lightOn || fanOn) ? HIGH : LOW);
  }
}

void publishStatus(const char* reason) {
  bool lightOnCopy = false;
  bool fanOnCopy = false;
  bool lightAutoCopy = true;
  bool luxOkCopy = false;
  float luxCopy = NAN;
  float tCopy = NAN;
  float hCopy = NAN;

  if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
  lightOnCopy = lightOn;
  fanOnCopy = fanOn;
  lightAutoCopy = lightAuto;
  luxOkCopy = lastLuxOk;
  luxCopy = lastLux;
  tCopy = lastTempC;
  hCopy = lastHum;
  if (stateMu) xSemaphoreGive(stateMu);

  String payload = "{\"node\":\"auto\",\"reason\":\"";
  payload += (reason ? reason : "unknown");
  payload += "\",\"light\":";
  payload += lightOnCopy ? "true" : "false";
  payload += ",\"light_auto\":";
  payload += lightAutoCopy ? "true" : "false";
  payload += ",\"fan\":";
  payload += fanOnCopy ? "true" : "false";
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
  payload += ",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  if (mqtt.connected()) {
    mqtt.publish(MQTT_TOPIC_STATUS, payload.c_str(), true);
  }
}

void logLight(uint32_t nowMs) {
  if (nextLightLogMs != 0 && nowMs < nextLightLogMs) return;
  nextLightLogMs = nowMs + LIGHT_LOG_MS;

  bool lightOnCopy = false;
  bool lightAutoCopy = true;
  bool luxOkCopy = false;
  float luxCopy = NAN;

  if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
  lightOnCopy = lightOn;
  lightAutoCopy = lightAuto;
  luxOkCopy = lastLuxOk;
  luxCopy = lastLux;
  if (stateMu) xSemaphoreGive(stateMu);

  Serial.print("[light] auto=");
  Serial.print(lightAutoCopy ? "1" : "0");
  Serial.print(" relay=");
  Serial.print(lightOnCopy ? "ON" : "OFF");
  Serial.print(" lux=");
  if (luxOkCopy) Serial.print(luxCopy, 1);
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
  const bool periodic = (nextPeriodicMs == 0 || now >= nextPeriodicMs);
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
  (void)topic;
  String cmd;
  cmd.reserve(length);
  for (unsigned int i = 0; i < length; ++i) cmd += static_cast<char>(payload[i]);
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "light auto") {
    if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
    lightAuto = true;
    if (stateMu) xSemaphoreGive(stateMu);
    publishStatus("light_auto");
  } else if (cmd == "light on") {
    if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
    lightAuto = false;
    lightOn = true;
    if (stateMu) xSemaphoreGive(stateMu);
    applyOutputs();
    publishStatus("light_on");
  } else if (cmd == "light off") {
    if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
    lightAuto = false;
    lightOn = false;
    if (stateMu) xSemaphoreGive(stateMu);
    applyOutputs();
    publishStatus("light_off");
  } else if (cmd == "fan on") {
    if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
    fanOn = true;
    if (stateMu) xSemaphoreGive(stateMu);
    applyOutputs();
    publishStatus("fan_on");
  } else if (cmd == "fan off") {
    if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
    fanOn = false;
    if (stateMu) xSemaphoreGive(stateMu);
    applyOutputs();
    publishStatus("fan_off");
  } else if (cmd == "status") {
    publishStatus("status");
  }
}

void connectWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) return;
  if (nowMs < nextWifiRetryMs) return;
  nextWifiRetryMs = nowMs + WIFI_RETRY_MS;
  if (strlen(WIFI_SSID) == 0) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectMqtt(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;
  if (nowMs < nextMqttRetryMs) return;
  nextMqttRetryMs = nowMs + MQTT_RETRY_MS;

  const bool hasAuth = strlen(MQTT_USERNAME) > 0;
  bool ok = false;

  if (hasAuth) {
    ok = mqtt.connect(
      MQTT_CLIENT_ID,
      MQTT_USERNAME,
      MQTT_PASSWORD,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"node\":\"auto\",\"reason\":\"offline\"}"
    );
  } else {
    ok = mqtt.connect(
      MQTT_CLIENT_ID,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"node\":\"auto\",\"reason\":\"offline\"}"
    );
  }

  if (!ok) return;
  mqtt.subscribe(MQTT_TOPIC_CMD);
  publishStatus("online");
}

void taskControl(void*) {
  uint32_t nextLuxMs = 0;
  uint32_t nextDhtMs = 0;

  for (;;) {
    const uint32_t now = millis();

    // Read lux and run local light automation.
    if (bhReady && (nextLuxMs == 0 || now >= nextLuxMs)) {
      nextLuxMs = now + AutoHw::LIGHT_SAMPLE_MS;

      float lux = NAN;
      const bool ok = bhReadLux(lux);

      bool doAuto = true;
      bool curLight = false;
      if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
      lastLuxOk = ok;
      lastLux = lux;
      doAuto = lightAuto;
      curLight = lightOn;
      if (stateMu) xSemaphoreGive(stateMu);

      if (doAuto && ok && !isnan(lux)) {
        bool newLight = curLight;
        if (!curLight && lux < AutoHw::LUX_ON) newLight = true;
        else if (curLight && lux > AutoHw::LUX_OFF) newLight = false;

        if (newLight != curLight) {
          if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
          lightOn = newLight;
          if (stateMu) xSemaphoreGive(stateMu);
          applyOutputs();
        }
      }
    }

    // Read DHT at a slower cadence.
    if (AutoHw::PIN_DHT != AutoHw::PIN_UNUSED && (nextDhtMs == 0 || now >= nextDhtMs)) {
      nextDhtMs = now + 2000;
      const float t = dht.readTemperature();
      const float h = dht.readHumidity();
      if (stateMu) xSemaphoreTake(stateMu, portMAX_DELAY);
      lastTempC = t;
      lastHum = h;
      if (stateMu) xSemaphoreGive(stateMu);
    }

    logLight(now);
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

    if (nextStatusMs == 0 || now >= nextStatusMs) {
      nextStatusMs = now + STATUS_PERIOD_MS;
      publishStatus("periodic");
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  stateMu = xSemaphoreCreateMutex();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt.setCallback(onMqttMessage);

  Wire.begin(AutoHw::PIN_I2C_SDA, AutoHw::PIN_I2C_SCL);
  if (bhInitAt(BH_ADDR_1)) {
    bhAddr = BH_ADDR_1;
    bhReady = true;
  } else if (bhInitAt(BH_ADDR_2)) {
    bhAddr = BH_ADDR_2;
    bhReady = true;
  }
  if (bhReady) {
    Serial.print("[auto] BH1750 OK addr=0x");
    Serial.println(bhAddr, HEX);
  } else {
    Serial.println("[auto] BH1750 not found (addr 0x23/0x5C)");
  }

  applyOutputs();
  if (AutoHw::PIN_DHT != AutoHw::PIN_UNUSED) dht.begin();

  const uint32_t now = millis();
  connectWifi(now);
  connectMqtt(now);
  publishStatus("boot");

  // Run control on core 1 (Arduino loop usually runs there), network on core 0.
  xTaskCreatePinnedToCore(taskControl, "auto_ctl", 4096, nullptr, 2, &taskControlHandle, 1);
  xTaskCreatePinnedToCore(taskNet, "auto_net", 6144, nullptr, 1, &taskNetHandle, 0);
}

void loop() {
  // Everything runs in FreeRTOS tasks.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
