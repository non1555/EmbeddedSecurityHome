#include <Arduino.h>

#include "app/App.h"
#include "app/Commands.h"
#include "app/Config.h"
#include "app/Events.h"
#include "app/HardwareConfig.h"
#include "app/RuleEngine.h"
#include "app/SystemState.h"
#include "drivers/UltrasonicDriver.h"
#include "sensors/ChokepointSensor.h"
#include "sensors/PirSensor.h"
#include "sensors/ReedSensor.h"
#include "sensors/VibrationSensor.h"

namespace {

RuleEngine engine;
SystemState state;
Config cfg;

// Test profile requested:
// Reed 2, Vibration 2, Motion(PIR) 3, Distance(US) 3
constexpr uint8_t REED_PINS[2] = {HwCfg::PIN_REED_1, 32};
constexpr uint8_t VIB_PINS[2] = {HwCfg::PIN_VIB_1, 36};
constexpr uint8_t PIR_PINS[3] = {HwCfg::PIN_PIR_1, 18, 19};

constexpr uint8_t US_TRIG_PINS[3] = {HwCfg::PIN_US_TRIG, 16, 4};
constexpr uint8_t US_ECHO_PINS[3] = {HwCfg::PIN_US_ECHO, 17, 5};

UltrasonicDriver us1(US_TRIG_PINS[0], US_ECHO_PINS[0]);
UltrasonicDriver us2(US_TRIG_PINS[1], US_ECHO_PINS[1]);
UltrasonicDriver us3(US_TRIG_PINS[2], US_ECHO_PINS[2]);

ChokepointSensor chokep1(&us1, 1, 35, 55, 200, 1500);
ChokepointSensor chokep2(&us2, 2, 35, 55, 200, 1500);
ChokepointSensor chokep3(&us3, 3, 35, 55, 200, 1500);

ReedSensor reed1(REED_PINS[0], 1, true, 80);
ReedSensor reed2(REED_PINS[1], 2, true, 80);

PirSensor pir1(PIR_PINS[0], 1, 1500);
PirSensor pir2(PIR_PINS[1], 2, 1500);
PirSensor pir3(PIR_PINS[2], 3, 1500);

VibrationSensor vib1(VIB_PINS[0], 1, 600, 700);
VibrationSensor vib2(VIB_PINS[1], 2, 600, 700);

uint32_t nextRawMs = 0;

const char* modeText(Mode m) {
  switch (m) {
    case Mode::disarm: return "disarm";
    case Mode::away: return "away";
    case Mode::night: return "night";
    default: return "unknown";
  }
}

const char* levelText(AlarmLevel lv) {
  switch (lv) {
    case AlarmLevel::off: return "off";
    case AlarmLevel::warn: return "warn";
    case AlarmLevel::alert: return "alert";
    case AlarmLevel::critical: return "critical";
    default: return "unknown";
  }
}

void printPins() {
  Serial.println("=== Sensor Pin Map ===");
  Serial.printf("REED1/REED2           : GPIO %u / %u\n", REED_PINS[0], REED_PINS[1]);
  Serial.printf("PIR1/PIR2/PIR3        : GPIO %u / %u / %u\n", PIR_PINS[0], PIR_PINS[1], PIR_PINS[2]);
  Serial.printf("VIB1/VIB2 (ADC)       : GPIO %u / %u\n", VIB_PINS[0], VIB_PINS[1]);
  Serial.printf("US1 TRIG/ECHO         : GPIO %u / %u\n", US_TRIG_PINS[0], US_ECHO_PINS[0]);
  Serial.printf("US2 TRIG/ECHO         : GPIO %u / %u\n", US_TRIG_PINS[1], US_ECHO_PINS[1]);
  Serial.printf("US3 TRIG/ECHO         : GPIO %u / %u\n", US_TRIG_PINS[2], US_ECHO_PINS[2]);
}

void printHelp() {
  Serial.println("=== test_sensor_logic ===");
  Serial.println("Manual event cases:");
  Serial.println("0=DISARM 1=ARM_NIGHT 6=ARM_AWAY");
  Serial.println("2=WINDOW_OPEN 7=DOOR_TAMPER 3=VIB_SPIKE 4=MOTION 5=CHOKEPOINT");
  Serial.println("m=print state r=reset state h=help p=pin map");
  Serial.println("Auto: polling REED/PIR/VIB/US and feeding RuleEngine");
  Serial.println("Note: buzzer/servo/mqtt/notify are not exercised in this test.");
}

void printState(const char* tag) {
  Serial.printf(
    "[%s] mode=%s level=%s last_notify_ms=%lu\n",
    tag,
    modeText(state.mode),
    levelText(state.level),
    (unsigned long)state.last_notify_ms
  );
}

void handleEvent(const Event& e, const char* srcLabel) {
  const Decision d = engine.handle(state, cfg, e);
  state = d.next;
  Serial.printf(
    "[EV:%s] type=%s src=%u -> mode=%s level=%s cmd=%s\n",
    srcLabel,
    toString(e.type),
    e.src,
    modeText(state.mode),
    levelText(state.level),
    toString(d.cmd.type)
  );
}

bool parseManualEvent(char c, Event& out) {
  const uint32_t nowMs = millis();
  if (c == '0') { out = {EventType::disarm, nowMs, 0}; return true; }
  if (c == '1') { out = {EventType::arm_night, nowMs, 0}; return true; }
  if (c == '6') { out = {EventType::arm_away, nowMs, 0}; return true; }
  if (c == '2') { out = {EventType::window_open, nowMs, 0}; return true; }
  if (c == '7') { out = {EventType::door_tamper, nowMs, 0}; return true; }
  if (c == '3') { out = {EventType::vib_spike, nowMs, 0}; return true; }
  if (c == '4') { out = {EventType::motion, nowMs, 0}; return true; }
  if (c == '5') { out = {EventType::chokepoint, nowMs, 0}; return true; }
  return false;
}

void pollManualInput() {
  if (!Serial.available()) return;
  const char c = (char)Serial.read();
  while (Serial.available()) {
    const char t = (char)Serial.read();
    if (t == '\n') break;
  }

  if (c == 'h') { printHelp(); return; }
  if (c == 'p') { printPins(); return; }
  if (c == 'm') { printState("STATE"); return; }
  if (c == 'r') {
    state = SystemState{};
    Serial.println("[RESET] state cleared");
    printState("STATE");
    return;
  }

  Event e{};
  if (!parseManualEvent(c, e)) {
    Serial.println("[?] unknown cmd, use h");
    return;
  }
  handleEvent(e, "manual");
}

void pollSensors() {
  const uint32_t nowMs = millis();
  Event e{};
  if (reed1.poll(nowMs, e)) handleEvent(e, "reed1");
  if (reed2.poll(nowMs, e)) handleEvent(e, "reed2");

  if (pir1.poll(nowMs, e)) handleEvent(e, "pir1");
  if (pir2.poll(nowMs, e)) handleEvent(e, "pir2");
  if (pir3.poll(nowMs, e)) handleEvent(e, "pir3");

  if (vib1.poll(nowMs, e)) handleEvent(e, "vib1");
  if (vib2.poll(nowMs, e)) handleEvent(e, "vib2");

  if (chokep1.poll(nowMs, e)) handleEvent(e, "us1");
  if (chokep2.poll(nowMs, e)) handleEvent(e, "us2");
  if (chokep3.poll(nowMs, e)) handleEvent(e, "us3");
}

void reportRaw() {
  const uint32_t nowMs = millis();
  if (nowMs < nextRawMs) return;
  nextRawMs = nowMs + 1000;

  const int reedRaw1 = digitalRead(REED_PINS[0]);
  const int reedRaw2 = digitalRead(REED_PINS[1]);
  const int pirRaw1 = digitalRead(PIR_PINS[0]);
  const int pirRaw2 = digitalRead(PIR_PINS[1]);
  const int pirRaw3 = digitalRead(PIR_PINS[2]);
  const int vibRaw1 = analogRead(VIB_PINS[0]);
  const int vibRaw2 = analogRead(VIB_PINS[1]);
  const int cm1 = us1.readCm();
  const int cm2 = us2.readCm();
  const int cm3 = us3.readCm();

  Serial.printf(
    "[RAW] reed=%d,%d pir=%d,%d,%d vib=%d,%d us_cm=%d,%d,%d | mode=%s level=%s\n",
    reedRaw1,
    reedRaw2,
    pirRaw1,
    pirRaw2,
    pirRaw3,
    vibRaw1,
    vibRaw2,
    cm1,
    cm2,
    cm3,
    modeText(state.mode),
    levelText(state.level)
  );
}

} // namespace

void App::begin() {
  Serial.println("APP: test_sensor_logic");
  printPins();
  printHelp();

  reed1.begin();
  reed2.begin();
  pir1.begin();
  pir2.begin();
  pir3.begin();
  vib1.begin();
  vib2.begin();
  us1.begin();
  us2.begin();
  us3.begin();
  chokep1.begin();
  chokep2.begin();
  chokep3.begin();

  nextRawMs = millis() + 300;
}

void App::tick(uint32_t) {
  pollManualInput();
  pollSensors();
  reportRaw();
}
