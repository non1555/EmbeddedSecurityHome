#pragma once

#include <Arduino.h>

#include "app/Commands.h"
#include "app/Events.h"
#include "app/SystemState.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace RtosQueues {

enum class PublishKind : uint8_t {
  event,
  status,
  ack
};

struct PublishMsg {
  PublishKind kind = PublishKind::event;
  Event e{};
  SystemState st{};
  Command cmd{CommandType::none, 0};
  bool ok = false;
  char text1[32]{};
  char text2[32]{};
};

struct CmdMsg {
  char payload[48]{};
};

struct ChokepointMsg {
  Event e{};
  int cm = -1;
};

#if defined(ARDUINO_ARCH_ESP32)
extern QueueHandle_t mqttPubQ;
extern QueueHandle_t mqttCmdQ;
extern QueueHandle_t chokepointQ;
#endif

bool init();

} // namespace RtosQueues

