#include <Arduino.h>

#include "app/App.h"
#include "app/Commands.h"
#include "app/Config.h"
#include "app/Events.h"
#include "app/HardwareConfig.h"
#include "app/RuleEngine.h"
#include "app/SystemState.h"

#if !KEYPAD_USE_I2C_EXPANDER
#include "drivers/KeypadDriver.h"
#else
#include <Wire.h>
#include "drivers/I2CKeypadDriver.h"
#endif

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "drivers/UltrasonicDriver.h"
#include "sensors/ChokepointSensor.h"
#include "sensors/PirSensor.h"
#include "sensors/ReedSensor.h"
#include "sensors/VibrationSensor.h"

namespace {

RuleEngine engine;
SystemState state;
Config cfg;

UltrasonicDriver us1(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
#if US_SENSOR_COUNT >= 2
UltrasonicDriver us2(HwCfg::PIN_US_TRIG_2, HwCfg::PIN_US_ECHO_2);
#endif
#if US_SENSOR_COUNT >= 3
UltrasonicDriver us3(HwCfg::PIN_US_TRIG_3, HwCfg::PIN_US_ECHO_3);
#endif
ChokepointSensor chokep1(&us1, 1, 35, 55, 200, 1500);
#if US_SENSOR_COUNT >= 2
ChokepointSensor chokep2(&us2, 2, 35, 55, 200, 1500);
#endif
#if US_SENSOR_COUNT >= 3
ChokepointSensor chokep3(&us3, 3, 35, 55, 200, 1500);
#endif
ReedSensor reedDoor(HwCfg::PIN_REED_1, 1, EventType::door_open, true, 80);
ReedSensor reedWindow(HwCfg::PIN_REED_2, 2, EventType::window_open, true, 80);
PirSensor pir1(HwCfg::PIN_PIR_1, 1, 1500);
PirSensor pir2(HwCfg::PIN_PIR_2, 2, 1500);
PirSensor pir3(HwCfg::PIN_PIR_3, 3, 1500);
VibrationSensor vib1(HwCfg::PIN_VIB_1, 1, 600, 700);
VibrationSensor vib2(HwCfg::PIN_VIB_2, 2, 600, 700);

Buzzer buzzer(HwCfg::PIN_BUZZER, 0);
Servo servo1(HwCfg::PIN_SERVO1, 1, 1, 10, 90);
#if SERVO_COUNT >= 2
Servo servo2(HwCfg::PIN_SERVO2, 2, 2, 10, 90);
#endif

#if !KEYPAD_USE_I2C_EXPANDER
KeypadDriver keypadDrv(HwCfg::KP_ROWS, 4, HwCfg::KP_COLS, 4, HwCfg::KP_MAP, 60);
#else
I2CKeypadDriver keypadDrv(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60);
#endif

uint32_t nextReportMs = 0;
char lastKey = 0;

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
  Serial.println("=== Board Pin Map ===");
  Serial.printf("BUZZER      : GPIO %u\n", HwCfg::PIN_BUZZER);
  Serial.printf("SERVO1      : GPIO %u\n", HwCfg::PIN_SERVO1);
#if SERVO_COUNT >= 2
  Serial.printf("SERVO2      : GPIO %u\n", HwCfg::PIN_SERVO2);
#endif
  Serial.printf("REED door/window      : GPIO %u / %u\n", HwCfg::PIN_REED_1, HwCfg::PIN_REED_2);
  Serial.printf("PIR 1/2/3             : GPIO %u / %u / %u\n", HwCfg::PIN_PIR_1, HwCfg::PIN_PIR_2, HwCfg::PIN_PIR_3);
  Serial.printf("VIB 1/2 (ADC)         : GPIO %u / %u\n", HwCfg::PIN_VIB_1, HwCfg::PIN_VIB_2);
  Serial.printf("US1 TRIG/ECHO         : GPIO %u / %u\n", HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
#if US_SENSOR_COUNT >= 2
  Serial.printf("US2 TRIG/ECHO         : GPIO %u / %u\n", HwCfg::PIN_US_TRIG_2, HwCfg::PIN_US_ECHO_2);
#endif
#if US_SENSOR_COUNT >= 3
  Serial.printf("US3 TRIG/ECHO         : GPIO %u / %u\n", HwCfg::PIN_US_TRIG_3, HwCfg::PIN_US_ECHO_3);
#endif
#if KEYPAD_USE_I2C_EXPANDER
  Serial.printf("I2C SDA/SCL : GPIO %u / %u\n", HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
  Serial.printf("KEYPAD I2C  : 0x%02X\n", HwCfg::KEYPAD_I2C_ADDR);
#else
  Serial.printf("KEYPAD ROWS : %u,%u,%u,%u\n", HwCfg::KP_ROWS[0], HwCfg::KP_ROWS[1], HwCfg::KP_ROWS[2], HwCfg::KP_ROWS[3]);
  Serial.printf("KEYPAD COLS : %u,%u,%u,%u\n", HwCfg::KP_COLS[0], HwCfg::KP_COLS[1], HwCfg::KP_COLS[2], HwCfg::KP_COLS[3]);
#endif
  Serial.printf("MANUAL BTN lock/unlock: GPIO %u / %u (active LOW)\n",
                HwCfg::PIN_BTN_MANUAL_LOCK,
                HwCfg::PIN_BTN_MANUAL_UNLOCK);
}

void printHelp() {
  Serial.println("=== test_board (board + logic) commands ===");
  Serial.println("h: help");
  Serial.println("p: print pin map");
  Serial.println("w: buzzer warn");
  Serial.println("a: buzzer alert");
  Serial.println("s: buzzer stop");
  Serial.println("l: servo lock");
  Serial.println("u: servo unlock");
  Serial.println("k: print last keypad key");
  Serial.println("m: print mode/level");
  Serial.println("r: reset state");
  Serial.println("0: DISARM 1: ARM_NIGHT 6: ARM_AWAY");
  Serial.println("8: DOOR_OPEN 2: WINDOW_OPEN 7: DOOR_TAMPER");
  Serial.println("3: VIB 4: MOTION 5: CHOKEPOINT");
  Serial.println("L: MANUAL_LOCK_REQUEST U: MANUAL_UNLOCK_REQUEST");
  Serial.println("Note: MQTT/Notify/RTOS queue are not exercised in this test.");
}

void printState(const char* tag) {
  Serial.printf(
    "[%s] mode=%s level=%s entry=%s deadline=%lu last_notify_ms=%lu\n",
    tag,
    modeText(state.mode),
    levelText(state.level),
    state.entry_pending ? "pending" : "none",
    (unsigned long)state.entry_deadline_ms,
    (unsigned long)state.last_notify_ms
  );
}

void applyDecision(const Event& e, const char* srcLabel) {
  if (e.type == EventType::manual_lock_request) {
    if (reedDoor.isOpen() || reedWindow.isOpen()) {
      Serial.println("[MANUAL] lock rejected: door/window open");
      return;
    }
    servo1.lock();
#if SERVO_COUNT >= 2
    servo2.lock();
#endif
    Serial.println("[MANUAL] lock accepted");
    return;
  }

  if (e.type == EventType::manual_unlock_request) {
    servo1.unlock();
#if SERVO_COUNT >= 2
    servo2.unlock();
#endif
    Serial.println("[MANUAL] unlock accepted");
    return;
  }

  const Decision d = engine.handle(state, cfg, e);
  state = d.next;

  Serial.printf(
    "[EV:%s] type=%s src=%u -> mode=%s level=%s entry=%s cmd=%s\n",
    srcLabel,
    toString(e.type),
    e.src,
    modeText(state.mode),
    levelText(state.level),
    state.entry_pending ? "pending" : "none",
    toString(d.cmd.type)
  );

  if (d.cmd.type == CommandType::buzzer_warn) buzzer.warn();
  if (d.cmd.type == CommandType::buzzer_alert) buzzer.alert();

  if (state.mode == Mode::disarm) {
    servo1.unlock();
#if SERVO_COUNT >= 2
    servo2.unlock();
#endif
  } else {
    servo1.lock();
#if SERVO_COUNT >= 2
    servo2.lock();
#endif
  }
}

bool parseManualEvent(char c, Event& out) {
  const uint32_t nowMs = millis();
  if (c == '0') { out = {EventType::disarm, nowMs, 0}; return true; }
  if (c == '1') { out = {EventType::arm_night, nowMs, 0}; return true; }
  if (c == '6') { out = {EventType::arm_away, nowMs, 0}; return true; }
  if (c == '8') { out = {EventType::door_open, nowMs, 0}; return true; }
  if (c == '2') { out = {EventType::window_open, nowMs, 0}; return true; }
  if (c == '7') { out = {EventType::door_tamper, nowMs, 0}; return true; }
  if (c == '3') { out = {EventType::vib_spike, nowMs, 0}; return true; }
  if (c == '4') { out = {EventType::motion, nowMs, 0}; return true; }
  if (c == '5') { out = {EventType::chokepoint, nowMs, 0}; return true; }
  if (c == 'L' || c == 'l') { out = {EventType::manual_lock_request, nowMs, 0}; return true; }
  if (c == 'U' || c == 'u') { out = {EventType::manual_unlock_request, nowMs, 0}; return true; }
  return false;
}

void handleSerial() {
  if (!Serial.available()) return;
  const char c = (char)Serial.read();
  while (Serial.available()) {
    const char t = (char)Serial.read();
    if (t == '\n') break;
  }

  if (c == 'h') { printHelp(); return; }
  if (c == 'p') { printPins(); return; }
  if (c == 'w') { buzzer.warn(); Serial.println("[ACT] buzzer warn"); return; }
  if (c == 'a') { buzzer.alert(); Serial.println("[ACT] buzzer alert"); return; }
  if (c == 's') { buzzer.stop(); Serial.println("[ACT] buzzer stop"); return; }
  if (c == 'l') {
    servo1.lock();
#if SERVO_COUNT >= 2
    servo2.lock();
#endif
    Serial.println("[ACT] servo lock");
    return;
  }
  if (c == 'u') {
    servo1.unlock();
#if SERVO_COUNT >= 2
    servo2.unlock();
#endif
    Serial.println("[ACT] servo unlock");
    return;
  }
  if (c == 'k') {
    Serial.printf("[KEYPAD] last key = %c\n", lastKey ? lastKey : '-');
    return;
  }
  if (c == 'm') { printState("STATE"); return; }
  if (c == 'r') {
    state = SystemState{};
    Serial.println("[RESET] state cleared");
    printState("STATE");
    return;
  }

  Event e{};
  if (parseManualEvent(c, e)) {
    applyDecision(e, "manual");
    return;
  }

  Serial.println("[?] unknown cmd, use h");
}

void pollSensors(uint32_t nowMs) {
  Event e{};
  if (reedDoor.poll(nowMs, e)) {
    applyDecision(e, "reed-door");
  }
  if (reedWindow.poll(nowMs, e)) {
    applyDecision(e, "reed-window");
  }
  if (pir1.poll(nowMs, e)) {
    applyDecision(e, "pir1");
  }
  if (pir2.poll(nowMs, e)) {
    applyDecision(e, "pir2");
  }
  if (pir3.poll(nowMs, e)) {
    applyDecision(e, "pir3");
  }
  if (vib1.poll(nowMs, e)) {
    applyDecision(e, "vib1");
  }
  if (vib2.poll(nowMs, e)) {
    applyDecision(e, "vib2");
  }
  if (chokep1.poll(nowMs, e)) {
    applyDecision(e, "us1");
  }
#if US_SENSOR_COUNT >= 2
  if (chokep2.poll(nowMs, e)) {
    applyDecision(e, "us2");
  }
#endif
#if US_SENSOR_COUNT >= 3
  if (chokep3.poll(nowMs, e)) {
    applyDecision(e, "us3");
  }
#endif
}

void pollKeypad(uint32_t nowMs) {
  const char k = keypadDrv.update(nowMs);
  if (!k) return;
  lastKey = k;
  Serial.printf("[KEYPAD] key=%c\n", k);
}

void periodicReport(uint32_t nowMs) {
  if (nowMs < nextReportMs) return;
  nextReportMs = nowMs + 1000;

  const int reedDoorRaw = digitalRead(HwCfg::PIN_REED_1);
  const int reedWindowRaw = digitalRead(HwCfg::PIN_REED_2);
  const int pirRaw1 = digitalRead(HwCfg::PIN_PIR_1);
  const int pirRaw2 = digitalRead(HwCfg::PIN_PIR_2);
  const int pirRaw3 = digitalRead(HwCfg::PIN_PIR_3);
  const int vibRaw1 = analogRead(HwCfg::PIN_VIB_1);
  const int vibRaw2 = analogRead(HwCfg::PIN_VIB_2);
  const int cm1 = us1.readCm();
#if US_SENSOR_COUNT >= 2
  const int cm2 = us2.readCm();
#else
  const int cm2 = -1;
#endif
#if US_SENSOR_COUNT >= 3
  const int cm3 = us3.readCm();
#else
  const int cm3 = -1;
#endif

  Serial.printf(
    "[RAW] reed=%d,%d pir=%d,%d,%d vib=%d,%d us=%d,%d,%d mode=%s level=%s entry=%s servo1=%s servo2=%s buzzer=%s\n",
    reedDoorRaw,
    reedWindowRaw,
    pirRaw1,
    pirRaw2,
    pirRaw3,
    vibRaw1,
    vibRaw2,
    cm1,
    cm2,
    cm3,
    modeText(state.mode),
    levelText(state.level),
    state.entry_pending ? "pending" : "none",
    servo1.isLocked() ? "lock" : "unlock",
#if SERVO_COUNT >= 2
    servo2.isLocked() ? "lock" : "unlock",
#else
    "-",
#endif
    buzzer.isActive() ? "on" : "off"
  );
}

} // namespace

void App::begin() {
  Serial.println("APP: test_board");
  printPins();
  printHelp();

#if KEYPAD_USE_I2C_EXPANDER
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
#endif
  keypadDrv.begin();

  reedDoor.begin();
  reedWindow.begin();
  pir1.begin();
  pir2.begin();
  pir3.begin();
  vib1.begin();
  vib2.begin();
  us1.begin();
#if US_SENSOR_COUNT >= 2
  us2.begin();
#endif
#if US_SENSOR_COUNT >= 3
  us3.begin();
#endif
  chokep1.begin();
#if US_SENSOR_COUNT >= 2
  chokep2.begin();
#endif
#if US_SENSOR_COUNT >= 3
  chokep3.begin();
#endif
  buzzer.begin();
  servo1.begin();
#if SERVO_COUNT >= 2
  servo2.begin();
#endif

  nextReportMs = millis() + 300;
}

void App::tick(uint32_t nowMs) {
  handleSerial();
  pollKeypad(nowMs);
  if (state.entry_pending && nowMs >= state.entry_deadline_ms) {
    Event te{EventType::entry_timeout, nowMs, 0};
    applyDecision(te, "timer");
  }
  pollSensors(nowMs);
  periodicReport(nowMs);

  buzzer.update(nowMs);
  servo1.update(nowMs);
#if SERVO_COUNT >= 2
  servo2.update(nowMs);
#endif
}

