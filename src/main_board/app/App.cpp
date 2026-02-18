#include "app/App.h"

#include "app/SecurityOrchestrator.h"

static SecurityOrchestrator orchestrator;

void App::begin() {
  orchestrator.begin();
}

void App::tick(uint32_t nowMs) {
  orchestrator.tick(nowMs);
}
