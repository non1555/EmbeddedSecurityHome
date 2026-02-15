#include "app/App.h"

#include "app/SecurityOrchestrator.h"
#include "automation/light_system.h"
#include "automation/temp_system.h"

static SecurityOrchestrator orchestrator;

void App::begin() {
  orchestrator.begin();
  initLightSystem();
  initTempSystem();
}

void App::tick(uint32_t nowMs) {
  orchestrator.tick(nowMs);
  (void)nowMs;
  updateLightSystem();
  updateTempSystem();
}
