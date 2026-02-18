#include "rtos/TaskRunner.h"

namespace TaskRunner {

void start(
  TaskFunction_t controlTask,
  TaskFunction_t netTask,
  TaskHandle_t* controlHandle,
  TaskHandle_t* netHandle
) {
  // Run control on core 1 (Arduino loop usually runs there), network on core 0.
  xTaskCreatePinnedToCore(controlTask, "auto_ctl", 4096, nullptr, 2, controlHandle, 1);
  xTaskCreatePinnedToCore(netTask, "auto_net", 6144, nullptr, 1, netHandle, 0);
}

} // namespace TaskRunner
