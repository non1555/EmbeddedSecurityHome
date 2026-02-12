#include <Arduino.h>

#include "app/App.h"
#include "app/Events.h"
#include "app/HardwareConfig.h"

#if !KEYPAD_USE_I2C_EXPANDER
#include "drivers/KeypadDriver.h"
#else
#include <Wire.h>
#include "drivers/I2CKeypadDriver.h"
#endif

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "drivers/UltrasonicDriver.h"
#include "sensors/PirSensor.h"
#include "sensors/ReedSensor.h"
#include "sensors/VibrationSensor.h"

namespace {

UltrasonicDriver us(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
ReedSensor reed(HwCfg::PIN_REED_1, 1, true, 80);
PirSensor pir(HwCfg::PIN_PIR_1, 1, 1500);
VibrationSensor vib(HwCfg::PIN_VIB_1, 1, 600, 700);

Buzzer buzzer(HwCfg::PIN_BUZZER, 0);
Servo servo1(HwCfg::PIN_SERVO1, 1, 1, 10, 90);
Servo servo2(HwCfg::PIN_SERVO2, 2, 2, 10, 90);

#if !KEYPAD_USE_I2C_EXPANDER
KeypadDriver keypadDrv(HwCfg::KP_ROWS, 4, HwCfg::KP_COLS, 4, HwCfg::KP_MAP, 60);
#else
I2CKeypadDriver keypadDrv(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60);
#endif

uint32_t nextReportMs = 0;
char lastKey = 0;

void printPins() {
  Serial.println("=== Board Pin Map ===");
  Serial.printf("BUZZER      : GPIO %u\n", HwCfg::PIN_BUZZER);
  Serial.printf("SERVO1      : GPIO %u\n", HwCfg::PIN_SERVO1);
  Serial.printf("SERVO2      : GPIO %u\n", HwCfg::PIN_SERVO2);
  Serial.printf("REED        : GPIO %u\n", HwCfg::PIN_REED_1);
  Serial.printf("PIR         : GPIO %u\n", HwCfg::PIN_PIR_1);
  Serial.printf("VIB (ADC)   : GPIO %u\n", HwCfg::PIN_VIB_1);
  Serial.printf("US TRIG/ECHO: GPIO %u / %u\n", HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
#if KEYPAD_USE_I2C_EXPANDER
  Serial.printf("I2C SDA/SCL : GPIO %u / %u\n", HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
  Serial.printf("KEYPAD I2C  : 0x%02X\n", HwCfg::KEYPAD_I2C_ADDR);
#else
  Serial.printf("KEYPAD ROWS : %u,%u,%u,%u\n", HwCfg::KP_ROWS[0], HwCfg::KP_ROWS[1], HwCfg::KP_ROWS[2], HwCfg::KP_ROWS[3]);
  Serial.printf("KEYPAD COLS : %u,%u,%u,%u\n", HwCfg::KP_COLS[0], HwCfg::KP_COLS[1], HwCfg::KP_COLS[2], HwCfg::KP_COLS[3]);
#endif
}

void printHelp() {
  Serial.println("=== test_board commands ===");
  Serial.println("h: help");
  Serial.println("p: print pin map");
  Serial.println("w: buzzer warn");
  Serial.println("a: buzzer alert");
  Serial.println("s: buzzer stop");
  Serial.println("l: servo lock");
  Serial.println("u: servo unlock");
  Serial.println("k: print last keypad key");
  Serial.println("Note: MQTT/Notify/RTOS queue not shown in this test, use Serial output first.");
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
    servo2.lock();
    Serial.println("[ACT] servo lock");
    return;
  }
  if (c == 'u') {
    servo1.unlock();
    servo2.unlock();
    Serial.println("[ACT] servo unlock");
    return;
  }
  if (c == 'k') {
    Serial.printf("[KEYPAD] last key = %c\n", lastKey ? lastKey : '-');
    return;
  }
  Serial.println("[?] unknown cmd, use h");
}

void pollSensors(uint32_t nowMs) {
  Event e{};
  if (reed.poll(nowMs, e)) {
    Serial.printf("[EVENT] %s src=%u\n", toString(e.type), e.src);
  }
  if (pir.poll(nowMs, e)) {
    Serial.printf("[EVENT] %s src=%u\n", toString(e.type), e.src);
  }
  if (vib.poll(nowMs, e)) {
    Serial.printf("[EVENT] %s src=%u\n", toString(e.type), e.src);
  }
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

  const int reedRaw = digitalRead(HwCfg::PIN_REED_1);
  const int pirRaw = digitalRead(HwCfg::PIN_PIR_1);
  const int vibRaw = analogRead(HwCfg::PIN_VIB_1);
  const int cm = us.readCm();

  Serial.printf(
    "[RAW] reed=%d pir=%d vib=%d us_cm=%d servo1=%s servo2=%s buzzer=%s\n",
    reedRaw,
    pirRaw,
    vibRaw,
    cm,
    servo1.isLocked() ? "lock" : "unlock",
    servo2.isLocked() ? "lock" : "unlock",
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

  reed.begin();
  pir.begin();
  vib.begin();
  us.begin();
  buzzer.begin();
  servo1.begin();
  servo2.begin();

  nextReportMs = millis() + 300;
}

void App::tick(uint32_t nowMs) {
  handleSerial();
  pollKeypad(nowMs);
  pollSensors(nowMs);
  periodicReport(nowMs);

  buzzer.update(nowMs);
  servo1.update(nowMs);
  servo2.update(nowMs);
}

