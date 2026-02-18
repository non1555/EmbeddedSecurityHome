#include "app/App.h"

#include "app/AutomationRuntime.h"

void App::begin() {
  AutoRuntime::begin();
}

void App::tick(uint32_t nowMs) {
  AutoRuntime::tick(nowMs);
}
