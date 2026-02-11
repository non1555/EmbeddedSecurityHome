#include "App.h"

#include <cstdio>

#include "app/Commands.h"
#include "app/Config.h"
#include "app/Events.h"
#include "app/HardwareConfig.h"
#include "app/MqttConfig.h"
#include "app/RuleEngine.h"
#include "app/SystemState.h"

#if !KEYPAD_USE_I2C_EXPANDER
#include "drivers/KeypadDriver.h"
#else
#include "drivers/I2CKeypadDriver.h"
#include <Wire.h>
#endif
#include "drivers/UltrasonicDriver.h"

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"

#include "sensors/AsyncChokepoint.h"
#include "sensors/ChokepointSensor.h"
#include "sensors/KeypadInput.h"
#include "sensors/PirSensor.h"
#include "sensors/ReedSensor.h"
#include "sensors/VibrationSensor.h"

#include "services/CommandDispatcher.h"
#include "services/Logger.h"
#include "services/MqttBus.h"
#include "services/Notify.h"

static RuleEngine engine;
static SystemState state;
static Config cfg;

static UltrasonicDriver us1(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
static ChokepointSensor chokep1(&us1, 1, 35, 55, 200, 1500);
static AsyncChokepoint asyncChokep(&chokep1);

static Buzzer buzzer(HwCfg::PIN_BUZZER, 0);
static Servo servo1(HwCfg::PIN_SERVO1, 1, 1, 10, 90);
static Servo servo2(HwCfg::PIN_SERVO2, 2, 2, 10, 90);

static Logger logger;
static Notify notifySvc;
static MqttBus mqttBus;

static Actuators acts{ &buzzer, &servo1, &servo2 };

static ReedSensor reed1(HwCfg::PIN_REED_1, 1, true, 80);
static PirSensor pir1(HwCfg::PIN_PIR_1, 1, 1500);
static VibrationSensor vib1(HwCfg::PIN_VIB_1, 1, 600, 700);

#if !KEYPAD_USE_I2C_EXPANDER
static KeypadDriver keypadDrv(HwCfg::KP_ROWS, 4, HwCfg::KP_COLS, 4, HwCfg::KP_MAP, 60);
#else
static I2CKeypadDriver keypadDrv(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60);
#endif
static KeypadInput keypadIn(0);

static void printEventDecision(const Event& e, const Decision& d) {
#if APP_VERBOSE_LOG
  Serial.print("[EV] ");
  Serial.print((int)e.type);
  Serial.print(" src=");
  Serial.print((int)e.src);
  Serial.print(" | [CMD] ");
  Serial.print((int)d.cmd.type);
  Serial.print(" | MODE ");
  Serial.print((int)d.next.mode);
  Serial.print(" | LEVEL ");
  Serial.println((int)d.next.level);
#else
  (void)e;
  (void)d;
#endif
}

static void handleEvent(const Event& e) {
  Decision d = engine.handle(state, cfg, e);
  state = d.next;
  applyCommand(d.cmd, state, acts, &notifySvc, &logger);
  mqttBus.publishEvent(e, state, d.cmd);
  mqttBus.publishStatus(state, "event");
  printEventDecision(e, d);
}

static bool parseKeyEvent(char c, uint32_t nowMs, Event& out) {
  if (c == '0') { out = {EventType::disarm, nowMs, 0}; return true; }
  if (c == '1') { out = {EventType::arm_night, nowMs, 0}; return true; }
  if (c == '6') { out = {EventType::arm_away, nowMs, 0}; return true; }
  if (c == '2') { out = {EventType::window_open, nowMs, 0}; return true; }
  if (c == '3') { out = {EventType::vib_spike, nowMs, 0}; return true; }
  if (c == '4') { out = {EventType::motion, nowMs, 0}; return true; }
  if (c == '5') { out = {EventType::chokepoint, nowMs, 0}; return true; }
  return false;
}

static bool readSerialEvent(uint32_t nowMs, Event& out) {
  if (!Serial.available()) return false;
  char c = (char)Serial.read();
  while (Serial.available()) {
    char t = (char)Serial.read();
    if (t == '\n') break;
  }
  return parseKeyEvent(c, nowMs, out);
}

static bool readSensorEvent(uint32_t nowMs, Event& out) {
  if (reed1.poll(nowMs, out)) return true;
  if (pir1.poll(nowMs, out)) return true;
  if (vib1.poll(nowMs, out)) return true;
  return false;
}

static void processMqttCommand(const String& payloadRaw) {
  String payload = payloadRaw;
  payload.trim();
  payload.toUpperCase();

  uint32_t nowMs = millis();
  Event e;

  if (payload == "DISARM") {
    e = {EventType::disarm, nowMs, 9};
    handleEvent(e);
    mqttBus.publishAck("DISARM", true, "ok");
    return;
  }
  if (payload == "ARM_NIGHT") {
    e = {EventType::arm_night, nowMs, 9};
    handleEvent(e);
    mqttBus.publishAck("ARM_NIGHT", true, "ok");
    return;
  }
  if (payload == "ARM_AWAY") {
    e = {EventType::arm_away, nowMs, 9};
    handleEvent(e);
    mqttBus.publishAck("ARM_AWAY", true, "ok");
    return;
  }
  if (payload == "STATUS") {
    mqttBus.publishStatus(state, "command");
    const auto st = mqttBus.stats();
    char detail[64];
    std::snprintf(
      detail,
      sizeof(detail),
      "d:%lu/%lu/%lu/%lu o:%lu s:%lu",
      (unsigned long)asyncChokep.dropCount(),
      (unsigned long)st.pubDrops,
      (unsigned long)st.cmdDrops,
      (unsigned long)st.storeDrops,
      (unsigned long)st.tickOverruns,
      (unsigned long)st.storeDepth
    );
    mqttBus.publishAck("STATUS", true, detail);
    return;
  }
  if (payload == "SERVO_LOCK") {
    if (acts.servo1) acts.servo1->lock();
    if (acts.servo2) acts.servo2->lock();
    mqttBus.publishStatus(state, "servo_lock");
    mqttBus.publishAck("SERVO_LOCK", true, "ok");
    return;
  }
  if (payload == "SERVO_UNLOCK") {
    if (acts.servo1) acts.servo1->unlock();
    if (acts.servo2) acts.servo2->unlock();
    mqttBus.publishStatus(state, "servo_unlock");
    mqttBus.publishAck("SERVO_UNLOCK", true, "ok");
    return;
  }

#if APP_VERBOSE_LOG
  Serial.print("[MQTT] unknown command: ");
  Serial.println(payloadRaw);
#endif
  mqttBus.publishAck(payloadRaw.c_str(), false, "unknown_command");
}

void App::begin() {
  logger.begin();
  notifySvc.begin();

#if KEYPAD_USE_I2C_EXPANDER
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
#endif
  keypadDrv.begin();
  keypadIn.begin();
  keypadIn.setArmCode("1234");
  keypadIn.setDisarmCode("0000");

  buzzer.begin();
  servo1.begin();
  servo2.begin();

  reed1.begin();
  pir1.begin();
  vib1.begin();
  us1.begin();
  chokep1.begin();
  asyncChokep.begin();
  mqttBus.begin();

  Serial.println("READY");
}

void App::tick(uint32_t nowMs) {
  mqttBus.update(nowMs);
  logger.update(nowMs);
  notifySvc.update(nowMs);
  mqttBus.setSensorTelemetry(asyncChokep.dropCount(), asyncChokep.queueDepth());

  char k = keypadDrv.update(nowMs);
  if (k) keypadIn.feedKey(k, nowMs);

  Event e;
  bool hasEvent = false;

  if (keypadIn.poll(nowMs, e)) {
    hasEvent = true;
  } else {
    int cm = -1;
    if (asyncChokep.poll(e, cm)) {
      hasEvent = true;
#if APP_VERBOSE_LOG
      Serial.print("[US] cm=");
      Serial.println(cm);
#endif
    }
  }

  String cmd;
  int burst = 0;
  while (burst < 4 && mqttBus.pollCommand(cmd)) {
    processMqttCommand(cmd);
    ++burst;
  }

  buzzer.update(nowMs);
  servo1.update(nowMs);
  servo2.update(nowMs);

  if (!hasEvent) hasEvent = readSensorEvent(nowMs, e);
  if (!hasEvent) hasEvent = readSerialEvent(nowMs, e);
  if (!hasEvent) return;

  handleEvent(e);
}

