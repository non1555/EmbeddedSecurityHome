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
QueueHandle_t eventQueue = nullptr;

void securityTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(ConfigurationSharedTypes::Config::RTOS_TICK_MS);
  constexpr uint8_t maxRemoteEventsPerTick = 2;

  for (;;) {
    esp_task_wdt_reset();

    const uint32_t nowMs = millis();
    ConfigurationSharedTypes::Event event{};

    // Network and remote-command path (flowchart section 3).
    if (!context.isMqttConnected()) {
      context.mqttTick(nowMs); // Non-blocking async reconnect attempt.
    } else {
      context.mqttTick(nowMs); // pollIncoming + publish drain.
      uint8_t remoteEvents = 0;
      while (remoteEvents < maxRemoteEventsPerTick && context.pollRemoteEvent(nowMs, event)) {
        engine.process(event, context);
        ++remoteEvents;
      }
    }

    // Keypad/sensor chain (flowchart section 4/5).
    if (collector.poll(nowMs, event)) {
      if (xQueueSend(eventQueue, &event, 0) != pdTRUE) {
        ++ConfigurationSharedTypes::RuntimeStats::securityEventDrops;
      }
    }

    if (context.pollTimeoutEvent(nowMs, event)) {
      if (xQueueSend(eventQueue, &event, 0) != pdTRUE) {
        ++ConfigurationSharedTypes::RuntimeStats::securityEventDrops;
      }
    }

    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      engine.process(event, context);
    }

    context.updateActuators(nowMs, engine);
    context.mqttTick(nowMs); // End-of-tick flush for pending status/event publish.
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
  if (esp_task_wdt_add(securityTaskHandle) != ESP_OK) {
    Serial.println("[BOOT] WDT add SecTask failed");
    delay(150);
    ESP.restart();
  }
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(200));
}
