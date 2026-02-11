#include <Arduino.h>

#include "app/App.h"
#include "app/RuleEngine.h"
#include "app/SystemState.h"
#include "app/Config.h"
#include "app/Events.h"
#include "app/Commands.h"

static RuleEngine engine;
static SystemState state;
static Config cfg;

static uint8_t currentSrc = 1;

static const char* cmdName(CommandType t) {
  switch (t) {
    case CommandType::none:         return "none";
    case CommandType::buzzer_warn:  return "buzzer_warn";
    case CommandType::buzzer_alert: return "buzzer_alert";
    case CommandType::notify:       return "notify";
    case CommandType::servo_lock:   return "servo_lock";
    default:                        return "?";
  }
}

static void printHelp() {
  Serial.println();
  Serial.println("=== APP SERIAL TEST (no wiring) ===");
  Serial.println("Events: 0=DISARM 1=ARM_NIGHT 2=WINDOW_OPEN 3=VIB 4=MOTION");
  Serial.println("Source: 5=src1 6=src2 7=src3");
  Serial.println("Other : 8=PRINT_STATE 9=RESET_STATE");
  Serial.println();
}

static bool readKey(char& out) {
  if (!Serial.available()) return false;
  out = (char)Serial.read();
  while (Serial.available()) { // flush to newline if any
    char t = (char)Serial.read();
    if (t == '\n') break;
  }
  return true;
}

static bool keyToEvent(char k, uint32_t nowMs, Event& out) {
  if (k == '0') { out = {EventType::disarm, nowMs, 0}; return true; }
  if (k == '1') { out = {EventType::arm_night, nowMs, 0}; return true; }
  if (k == '2') { out = {EventType::window_open, nowMs, currentSrc}; return true; }
  if (k == '3') { out = {EventType::vib_spike, nowMs, currentSrc}; return true; }
  if (k == '4') { out = {EventType::motion, nowMs, currentSrc}; return true; }
  return false;
}

static void printState() {
  Serial.print("[STATE] MODE=");
  Serial.print((int)state.mode);
  Serial.print(" LEVEL=");
  Serial.print((int)state.level);
  Serial.print(" src=");
  Serial.println((int)currentSrc);
}

void App::begin() {
  // ต่อให้ main เรียก Serial.begin แล้ว เรียกซ้ำก็ไม่พัง
  Serial.begin(115200);
  delay(200);

  Serial.println("READY (AppSerialAsApp)");
  printHelp();
  printState();
}

void App::tick(uint32_t nowMs) {
  char k;
  if (!readKey(k)) return;

  if (k == '5') { currentSrc = 1; Serial.println("[SRC] 1"); return; }
  if (k == '6') { currentSrc = 2; Serial.println("[SRC] 2"); return; }
  if (k == '7') { currentSrc = 3; Serial.println("[SRC] 3"); return; }

  if (k == '8') { printState(); return; }
  if (k == '9') {
    state = SystemState{};
    Serial.println("[RESET] state cleared");
    printState();
    return;
  }

  Event e;
  if (!keyToEvent(k, nowMs, e)) {
    Serial.println("[?] unknown key");
    printHelp();
    return;
  }

  Decision d = engine.handle(state, cfg, e);
  state = d.next;

  Serial.print("[EV] ");
  Serial.print((int)e.type);
  Serial.print(" src=");
  Serial.print((int)e.src);

  Serial.print(" | [CMD] ");
  Serial.print((int)d.cmd.type);
  Serial.print(" (");
  Serial.print(cmdName(d.cmd.type));
  Serial.print(")");

  Serial.print(" | NEXT MODE=");
  Serial.print((int)state.mode);
  Serial.print(" LEVEL=");
  Serial.println((int)state.level);
}
