#pragma once

#include <Arduino.h>

namespace TaskRunner {

void start(
  TaskFunction_t controlTask,
  TaskFunction_t netTask,
  TaskHandle_t* controlHandle,
  TaskHandle_t* netHandle
);

} // namespace TaskRunner
