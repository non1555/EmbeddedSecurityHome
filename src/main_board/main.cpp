#include <Arduino.h>
#include <esp_task_wdt.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "application_logic_layer/EventCollector.h"
#include "application_logic_layer/RuleEngine.h"
#include "application_logic_layer/SystemContext.h"
#include "configuration_shared_types/Config.h"
#include "configuration_shared_types/RuntimeStats.h"

namespace {

ApplicationLogicLayer::SystemContext context;
ApplicationLogicLayer::EventCollector collector;
ApplicationLogicLayer::RuleEngine engine;

TaskHandle_t securityTaskHandle = nullptr;
TaskHandle_t mqttTaskHandle = nullptr;
QueueHandle_t eventQueue = nullptr;

void printSerialEvent(const char* path, const ConfigurationSharedTypes::Event& event) {
  Serial.print("[SERIAL] ");
  Serial.print(path);
  Serial.print(" event=");
  Serial.print(ConfigurationSharedTypes::toString(event.type));
  Serial.print(" src=");
  Serial.println(event.src);
}

void printSerialStateChanges(const ConfigurationSharedTypes::SystemState& prev,
                             const ConfigurationSharedTypes::SystemState& curr) {
  auto printBoolField = [](const char* name, bool value) {
    Serial.print("[SERIAL] state ");
    Serial.print(name);
    Serial.print("=");
    Serial.println(value ? 1 : 0);
  };

  if (prev.mode != curr.mode) {
    Serial.print("[SERIAL] state mode=");
    Serial.println(ConfigurationSharedTypes::toString(curr.mode));
  }
  if (prev.latest_mode != curr.latest_mode) {
    Serial.print("[SERIAL] state latest_mode=");
    Serial.println(ConfigurationSharedTypes::toString(curr.latest_mode));
  }
  if (prev.level != curr.level) {
    Serial.print("[SERIAL] state level=");
    Serial.println(ConfigurationSharedTypes::toString(curr.level));
  }
  if (prev.is_night != curr.is_night) {
    printBoolField("is_night", curr.is_night);
  }
  if (prev.failed_attempts != curr.failed_attempts) {
    Serial.print("[SERIAL] state failed_attempts=");
    Serial.println(curr.failed_attempts);
  }
  if (prev.exit_stage != curr.exit_stage) {
    Serial.print("[SERIAL] state exit_stage=");
    Serial.println(curr.exit_stage);
  }
  if (prev.door_locked != curr.door_locked) {
    printBoolField("door_locked", curr.door_locked);
  }
  if (prev.window_locked != curr.window_locked) {
    printBoolField("window_locked", curr.window_locked);
  }
  if (prev.door_open != curr.door_open) {
    printBoolField("door_open", curr.door_open);
  }
  if (prev.window_open != curr.window_open) {
    printBoolField("window_open", curr.window_open);
  }
  if (!(prev.door_locked && prev.door_open) &&
      (curr.door_locked && curr.door_open)) {
    Serial.println("[SERIAL] alert door_open_while_locked");
  }
  if (!(prev.window_locked && prev.window_open) &&
      (curr.window_locked && curr.window_open)) {
    Serial.println("[SERIAL] alert window_open_while_locked");
  }
}

void securityTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(ConfigurationSharedTypes::Config::RTOS_TICK_MS);
  constexpr uint8_t maxRemoteEventsPerTick = 1;
  ConfigurationSharedTypes::SystemState lastSerialState = context.state();

  for (;;) {
    esp_task_wdt_reset();

    const uint32_t nowMs = millis();
    ConfigurationSharedTypes::Event event{};

    // Remote commands are drained here; network reconnect lives in mqttTask.
    uint8_t remoteEvents = 0;
    while (remoteEvents < maxRemoteEventsPerTick && context.pollRemoteEvent(nowMs, event)) {
      engine.process(event, context);
      printSerialEvent("remote", event);
      ++remoteEvents;
    }

    // Keypad/sensor chain (flowchart section 4/5).
    if (collector.poll(nowMs, event)) {
      if (xQueueSend(eventQueue, &event, 0) != pdTRUE) {
        ++ConfigurationSharedTypes::RuntimeStats::securityEventDrops;
      }
    }

    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      engine.process(event, context);
      printSerialEvent("local", event);
    }

    context.updateActuators(nowMs, engine);
    printSerialStateChanges(lastSerialState, context.state());
    lastSerialState = context.state();
    vTaskDelayUntil(&lastWake, period);
  }
}

void mqttTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(ConfigurationSharedTypes::Config::MQTT_TASK_MS);

  for (;;) {
    context.mqttTick(millis());
    vTaskDelayUntil(&lastWake, period);
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  if (esp_task_wdt_init(3, true) != ESP_OK) {
    Serial.println("[BOOT] WDT init failed");
    delay(150);
    ESP.restart();
  }

  if (!context.begin()) {
    Serial.println("[BOOT] context begin failed");
    delay(150);
    ESP.restart();
  }

  collector.begin();
  context.bindCollector(&collector);
  eventQueue = xQueueCreate(24, sizeof(ConfigurationSharedTypes::Event));
  if (!eventQueue) {
    Serial.println("[BOOT] Event queue init failed");
    delay(150);
    ESP.restart();
  }

  auto createTaskOrRestart = [](BaseType_t result, const char* taskName) {
    if (result == pdPASS) return;
    Serial.print("[BOOT] Task Failed: ");
    Serial.println(taskName);
    delay(150);
    ESP.restart();
  };

  createTaskOrRestart(
    xTaskCreatePinnedToCore(securityTask, "SecTask", 8192, nullptr, 2, &securityTaskHandle, 1),
    "SecTask");
  createTaskOrRestart(
    xTaskCreatePinnedToCore(mqttTask, "MqttTask", 6144, nullptr, 1, &mqttTaskHandle, 0),
    "MqttTask");
  if (esp_task_wdt_add(securityTaskHandle) != ESP_OK) {
    Serial.println("[BOOT] WDT add SecTask failed");
    delay(150);
    ESP.restart();
  }
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(200));
}
