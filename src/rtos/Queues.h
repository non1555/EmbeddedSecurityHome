#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "app/Events.h"
#include "app/Commands.h"
#include "app/SystemState.h"

namespace RtosQueues {

struct CommandMsg {
  Command cmd;
  SystemState st;
};

extern QueueHandle_t eventQ; // item: Event
extern QueueHandle_t cmdQ;   // item: CommandMsg (length 1)

bool init();                 // create queues

} // namespace
