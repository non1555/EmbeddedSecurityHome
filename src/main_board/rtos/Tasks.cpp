#include "rtos/Tasks.h"

#include <cstdio>
#include <cstring>

#include "rtos/Queues.h"

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace RtosTasks {

static MqttClient* gMqtt = nullptr;
static ChokepointSensor* gChokepoint = nullptr;

static TaskHandle_t hMqtt = nullptr;
static TaskHandle_t hChokepoint = nullptr;
static bool mqttStarted = false;
static bool chokepointStarted = false;

static volatile uint32_t gPubDrops = 0;
static volatile uint32_t gCmdDrops = 0;
static volatile uint32_t gStoreDrops = 0;
static volatile uint32_t gTickOverruns = 0;
static volatile uint32_t gStoreDepth = 0;
static volatile uint32_t gSensorDrops = 0;
static volatile uint32_t gSensorDepth = 0;

static Preferences pref;
static bool prefReady = false;
static RtosQueues::PublishMsg store[MQTT_STORE_CAP];
static uint32_t storeHead = 0;
static uint32_t storeTail = 0;
static uint32_t storeCount = 0;

static inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

static void copyText(char* dst, size_t dstLen, const char* src) {
  if (!dst || dstLen == 0) return;
  dst[0] = '\0';
  if (!src) return;
  std::strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
}

static void slotKey(uint32_t idx, char* out, size_t outLen) {
  std::snprintf(out, outLen, "s%02lu", (unsigned long)idx);
}

static void persistMeta() {
  if (!prefReady) return;
  pref.putUInt("h", storeHead);
  pref.putUInt("t", storeTail);
  pref.putUInt("c", storeCount);
}

static void persistSlot(uint32_t idx, const RtosQueues::PublishMsg& msg) {
  if (!prefReady) return;
  char key[8];
  slotKey(idx, key, sizeof(key));
  pref.putBytes(key, &msg, sizeof(RtosQueues::PublishMsg));
}

static void resetStore() {
  storeHead = 0;
  storeTail = 0;
  storeCount = 0;
  persistMeta();
}

static void loadStore() {
  prefReady = pref.begin("eshmqv1", false);
  if (!prefReady) {
    resetStore();
    return;
  }

  storeHead = pref.getUInt("h", 0);
  storeTail = pref.getUInt("t", 0);
  storeCount = pref.getUInt("c", 0);

  if (storeHead >= MQTT_STORE_CAP || storeTail >= MQTT_STORE_CAP || storeCount > MQTT_STORE_CAP) {
    resetStore();
  }

  for (uint32_t i = 0; i < MQTT_STORE_CAP; ++i) {
    char key[8];
    slotKey(i, key, sizeof(key));
    if (pref.getBytesLength(key) == sizeof(RtosQueues::PublishMsg)) {
      pref.getBytes(key, &store[i], sizeof(RtosQueues::PublishMsg));
    }
  }
}

static bool storePush(const RtosQueues::PublishMsg& msg) {
  if (storeCount >= MQTT_STORE_CAP) return false;
  store[storeTail] = msg;
  persistSlot(storeTail, msg);
  storeTail = (storeTail + 1) % MQTT_STORE_CAP;
  ++storeCount;
  persistMeta();
  return true;
}

static bool storePeek(RtosQueues::PublishMsg& out) {
  if (storeCount == 0) return false;
  out = store[storeHead];
  return true;
}

static void storePop() {
  if (storeCount == 0) return;
  storeHead = (storeHead + 1) % MQTT_STORE_CAP;
  --storeCount;
  persistMeta();
}

static bool publishMsg(const RtosQueues::PublishMsg& msg) {
  if (!gMqtt) return false;
  switch (msg.kind) {
    case RtosQueues::PublishKind::event:
      return gMqtt->publishEvent(msg.e, msg.st, msg.cmd);
    case RtosQueues::PublishKind::status:
      return gMqtt->publishStatus(msg.st, msg.text1);
    case RtosQueues::PublishKind::ack:
      return gMqtt->publishAck(msg.text1, msg.ok, msg.text2);
    default:
      return false;
  }
}

static void onMqttCommand(const String&, const String& payloadRaw) {
  if (!RtosQueues::mqttCmdQ) return;
  RtosQueues::CmdMsg msg{};
  payloadRaw.toCharArray(msg.payload, sizeof(msg.payload));
  if (xQueueSend(RtosQueues::mqttCmdQ, &msg, 0) != pdTRUE) {
    ++gCmdDrops;
  }
}

static void mqttTask(void*) {
  if (!gMqtt) vTaskDelete(nullptr);

  loadStore();
  gMqtt->begin(onMqttCommand);

  const TickType_t period = pdMS_TO_TICKS(10);
  TickType_t last = xTaskGetTickCount();
  uint32_t nextMetricsMs = 0;

  for (;;) {
    const uint32_t nowMs = millis();
    gMqtt->update(nowMs);

    if (gMqtt->ready()) {
      RtosQueues::PublishMsg msg{};
      uint32_t burst = 0;
      while (burst < MQTT_STORE_FLUSH_BURST && storePeek(msg)) {
        if (!publishMsg(msg)) break;
        storePop();
        ++burst;
      }
    }

    if (RtosQueues::mqttPubQ) {
      RtosQueues::PublishMsg msg{};
      uint32_t burst = 0;
      while (burst < MQTT_PUB_DRAIN_BURST && xQueueReceive(RtosQueues::mqttPubQ, &msg, 0) == pdTRUE) {
        if (gMqtt->ready() && storeCount == 0) {
          if (!publishMsg(msg) && !storePush(msg)) {
            ++gStoreDrops;
          }
        } else if (!storePush(msg)) {
          ++gStoreDrops;
        }
        ++burst;
      }
    }

    if (reached(nowMs, nextMetricsMs)) {
      nextMetricsMs = nowMs + MQTT_METRICS_PERIOD_MS;
      gMqtt->publishMetrics(
        gSensorDrops,
        gPubDrops,
        gCmdDrops,
        gStoreDrops,
        gSensorDepth,
        RtosQueues::mqttPubQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttPubQ) : 0,
        RtosQueues::mqttCmdQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttCmdQ) : 0,
        storeCount
      );
    }

    gStoreDepth = storeCount;

    const TickType_t nowTicks = xTaskGetTickCount();
    if ((nowTicks - last) > period) {
      ++gTickOverruns;
    }
    vTaskDelayUntil(&last, period);
  }
}

static void chokepointTask(void*) {
  const TickType_t period = pdMS_TO_TICKS(10);
  TickType_t last = xTaskGetTickCount();

  for (;;) {
    Event e;
    const uint32_t nowMs = millis();
    if (gChokepoint && gChokepoint->poll(nowMs, e)) {
      RtosQueues::ChokepointMsg msg{};
      msg.e = e;
      msg.cm = gChokepoint->lastCm();
      if (RtosQueues::chokepointQ && xQueueSend(RtosQueues::chokepointQ, &msg, 0) != pdTRUE) {
        ++gSensorDrops;
      }
    }
    gSensorDepth = RtosQueues::chokepointQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::chokepointQ) : 0;
    vTaskDelayUntil(&last, period);
  }
}

void attachMqtt(MqttClient* client) {
  gMqtt = client;
}

void attachChokepoint(ChokepointSensor* sensor) {
  gChokepoint = sensor;
}

void startIfReady() {
  if (!RtosQueues::init()) return;

  if (gMqtt && !mqttStarted) {
    if (xTaskCreatePinnedToCore(mqttTask, "Mqtt", 4096, nullptr, 1, &hMqtt, 0) == pdPASS) {
      mqttStarted = true;
    }
  }

  if (gChokepoint && !chokepointStarted) {
    if (xTaskCreatePinnedToCore(chokepointTask, "USonic", 3072, nullptr, 1, &hChokepoint, 1) == pdPASS) {
      chokepointStarted = true;
    }
  }
}

void setSensorTelemetry(uint32_t drops, uint32_t depth) {
  gSensorDrops = drops;
  gSensorDepth = depth;
}

Stats stats() {
  Stats s{};
  s.pubDrops = gPubDrops;
  s.cmdDrops = gCmdDrops;
  s.storeDrops = gStoreDrops;
  s.tickOverruns = gTickOverruns;
  s.storeDepth = gStoreDepth;
  s.sensorDrops = gSensorDrops;
  s.sensorDepth = gSensorDepth;
  return s;
}

bool enqueuePublish(const RtosQueues::PublishMsg& msg) {
  if (!RtosQueues::mqttPubQ) return false;
  if (xQueueSend(RtosQueues::mqttPubQ, &msg, 0) != pdTRUE) {
    ++gPubDrops;
    return false;
  }
  return true;
}

bool dequeueCommand(RtosQueues::CmdMsg& out) {
  if (!RtosQueues::mqttCmdQ) return false;
  return xQueueReceive(RtosQueues::mqttCmdQ, &out, 0) == pdTRUE;
}

bool dequeueChokepoint(RtosQueues::ChokepointMsg& out) {
  if (!RtosQueues::chokepointQ) return false;
  return xQueueReceive(RtosQueues::chokepointQ, &out, 0) == pdTRUE;
}

} // namespace RtosTasks
