#include "rtos/Queues.h"

namespace RtosQueues {

#if defined(ARDUINO_ARCH_ESP32)
QueueHandle_t mqttPubQ = nullptr;
QueueHandle_t mqttCmdQ = nullptr;
QueueHandle_t chokepointQ = nullptr;
#endif

bool init() {
#if defined(ARDUINO_ARCH_ESP32)
  if (!mqttPubQ) mqttPubQ = xQueueCreate(16, sizeof(PublishMsg));
  if (!mqttCmdQ) mqttCmdQ = xQueueCreate(8, sizeof(CmdMsg));
  if (!chokepointQ) chokepointQ = xQueueCreate(8, sizeof(ChokepointMsg));
  return mqttPubQ && mqttCmdQ && chokepointQ;
#else
  return false;
#endif
}

} // namespace RtosQueues

