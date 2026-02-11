#include "App.h"

#include "app/RuleEngine.h"
#include "app/SystemState.h"
#include "app/Config.h"
#include "app/Events.h"
#include "app/Commands.h"
#include "app/HardwareConfig.h"

#if !KEYPAD_USE_I2C_EXPANDER
#include "drivers/KeypadDriver.h"
#else
#include "drivers/I2CKeypadDriver.h"
#include <Wire.h>
#endif
#include "drivers/UltrasonicDriver.h"

#include "sensors/ChokepointSensor.h"
#include "sensors/KeypadInput.h"
#include "sensors/ReedSensor.h"
#include "sensors/PirSensor.h"
#include "sensors/VibrationSensor.h"

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"
#include "services/CommandDispatcher.h"
#include "services/Logger.h"
#include "services/Notify.h"

static RuleEngine engine;
static SystemState state;
static Config cfg;

static UltrasonicDriver us1(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
static ChokepointSensor chokep1(&us1, 1, 35, 55, 200, 1500);

// ===== ACTUATORS =====
static Buzzer buzzer(HwCfg::PIN_BUZZER, 0);
static Servo  servo1(HwCfg::PIN_SERVO1, 1, 1, 10, 90);
static Servo  servo2(HwCfg::PIN_SERVO2, 2, 2, 10, 90);

static Logger logger;
static Notify notifySvc;

static Actuators acts{ &buzzer, &servo1, &servo2 };

// ===== SENSORS (รองรับหลายตัวด้วย id) =====
static ReedSensor reed1(HwCfg::PIN_REED_1, 1, true, 80);
static PirSensor  pir1(HwCfg::PIN_PIR_1,  1, 1500);
static VibrationSensor vib1(HwCfg::PIN_VIB_1, 1, 600, 700);

#if !KEYPAD_USE_I2C_EXPANDER
static KeypadDriver keypadDrv(HwCfg::KP_ROWS, 4, HwCfg::KP_COLS, 4, HwCfg::KP_MAP, 60);
#else
static I2CKeypadDriver keypadDrv(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60);
#endif
static KeypadInput  keypadIn(0);

// ===== serial key fallback (ไว้เทส/ดีบัก) =====
static bool parseKeyEvent(char c, uint32_t nowMs, Event& out) {
  if (c == '0') { out = {EventType::disarm, nowMs, 0}; return true; }
  if (c == '1') { out = {EventType::arm_night, nowMs, 0}; return true; }
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
  // ลำดับนี้คือ priority ง่าย ๆ: งัดก่อน → คนเดิน → สั่น
  if (reed1.poll(nowMs, out)) return true;
  if (pir1.poll(nowMs, out))  return true;
  if (vib1.poll(nowMs, out))  return true;
  return false;
}

static void printEventDecision(const Event& e, const Decision& d) {
  Serial.print("[EV] "); Serial.print((int)e.type);
  Serial.print(" src="); Serial.print((int)e.src);
  Serial.print(" | [CMD] "); Serial.print((int)d.cmd.type);
  Serial.print(" | MODE "); Serial.print((int)d.next.mode);
  Serial.print(" | LEVEL "); Serial.println((int)d.next.level);
}

void App::begin() {
  logger.begin();
  notifySvc.begin();

#if KEYPAD_USE_I2C_EXPANDER
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
#endif
  keypadDrv.begin();
  keypadIn.begin();
  keypadIn.setArmCode("1234");     // เปลี่ยนรหัสได้ตรงนี้
  keypadIn.setDisarmCode("0000");  // เปลี่ยนรหัสได้ตรงนี้

  buzzer.begin();
  servo1.begin();
  servo2.begin();
5
  reed1.begin();
  pir1.begin();
  vib1.begin();
  us1.begin();
  chokep1.begin();

  Serial.println("READY");
  Serial.println("Keys: 0=DISARM 1=ARM_NIGHT 2=WINDOW_OPEN 3=VIB 4=MOTION 5=CHOKEPOINT");
#if KEYPAD_USE_I2C_EXPANDER
  Serial.println("Keypad input: I2C expander mode (PCF8574 scan active)");
#endif
}

void App::tick(uint32_t nowMs) {
  char k = keypadDrv.update(nowMs);
  if (k) keypadIn.feedKey(k, nowMs);

  Event ke;
  if (keypadIn.poll(nowMs, ke)) {
    Decision d = engine.handle(state, cfg, ke);
    state = d.next;
    applyCommand(d.cmd, state, acts, &notifySvc, &logger);
    return;
  }

  Event ue;
  if (chokep1.poll(nowMs, ue)) {
    Decision d = engine.handle(state, cfg, ue);
    state = d.next;
    applyCommand(d.cmd, state, acts, &notifySvc, &logger);

    Serial.print("[US] cm="); Serial.println(chokep1.lastCm());
    return;
  }

  buzzer.update(nowMs);
  servo1.update(nowMs);
  servo2.update(nowMs);

  Event e;
  bool hasEvent = readSensorEvent(nowMs, e);

  // ถ้าไม่มี event จาก sensor ค่อยให้ serial เป็น fallback
  if (!hasEvent) hasEvent = readSerialEvent(nowMs, e);
  if (!hasEvent) return;

  Decision d = engine.handle(state, cfg, e);
  state = d.next;

  applyCommand(d.cmd, state, acts, &notifySvc, &logger);

  printEventDecision(e, d);
}
