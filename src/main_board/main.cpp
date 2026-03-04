#include <Arduino.h>
#include <esp_task_wdt.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "application_logic_layer/EventCollector.h"
#include "application_logic_layer/RuleEngine.h"
#include "application_logic_layer/SystemContext.h"
#include "configuration_shared_types/Config.h"

namespace {

ApplicationLogicLayer::SystemContext context;
ApplicationLogicLayer::EventCollector collector;
ApplicationLogicLayer::RuleEngine engine;

TaskHandle_t securityTaskHandle = nullptr;
TaskHandle_t mqttTaskHandle = nullptr;
QueueHandle_t eventQueue = nullptr;

void securityTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(ConfigurationSharedTypes::Config::RTOS_TICK_MS);
  constexpr uint8_t maxRemoteEventsPerTick = 1;

  for (;;) {
    esp_task_wdt_reset();

    const uint32_t nowMs = millis();
    context.securityTick(nowMs, engine, eventQueue, maxRemoteEventsPerTick);
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
