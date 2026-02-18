#include "rtos/Queues.h"

namespace RtosQueues {

QueueHandle_t mqttPubQ = nullptr;
QueueHandle_t mqttCmdQ = nullptr;
QueueHandle_t chokepointQ = nullptr;

bool init() {
  if (!mqttPubQ) mqttPubQ = xQueueCreate(16, sizeof(PublishMsg));
  if (!mqttCmdQ) mqttCmdQ = xQueueCreate(8, sizeof(CmdMsg));
  if (!chokepointQ) chokepointQ = xQueueCreate(8, sizeof(ChokepointMsg));
  return mqttPubQ && mqttCmdQ && chokepointQ;
}

} // namespace RtosQueues
