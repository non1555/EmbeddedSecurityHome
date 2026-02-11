#include "app/App.h"
#include "app/RuleEngine.h"

static RuleEngine engine;
static SystemState state;
static Config cfg;

static void flushLine() {
  while (Serial.available()) {
    char t = (char)Serial.read();
    if (t == '\n') break;
  }
}

void App::begin() {
  Serial.println("APP: test_step1");
  Serial.println("0 1 2 3 4");
  Serial.println("7 8 9");
}

void App::tick(uint32_t) {
  if (!Serial.available()) return;

  char c = (char)Serial.read();
  flushLine();
  uint32_t nowMs = millis();

  if (c == '8') { state.mode  = (Mode)2; Serial.println("[SET] MODE=2"); return; }
  if (c == '9') { state.mode  = (Mode)0; Serial.println("[SET] MODE=0"); return; }
  if (c == '7') { state.level = (AlarmLevel)0; Serial.println("[SET] LEVEL=0"); return; }

  EventType et;
  if (c == '0') et = EventType::disarm;
  else if (c == '1') et = EventType::arm_night;
  else if (c == '2') et = EventType::window_open;
  else if (c == '3') et = EventType::vib_spike;
  else if (c == '4') et = EventType::arm_away;
  else { Serial.println("Use 0-4,7-9"); return; }

  Event e{et, nowMs};
  Decision d = engine.handle(state, cfg, e);
  state = d.next;

  Serial.print("[KEY] ");   Serial.print(c);
  Serial.print(" | MODE "); Serial.print((int)state.mode);
  Serial.print(" | LEVEL ");Serial.print((int)state.level);
  Serial.print(" | CMD ");  Serial.println((int)d.cmd.type);
}
