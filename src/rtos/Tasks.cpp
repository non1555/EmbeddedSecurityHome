#if !defined(ARDUINO_ARCH_ESP32)
  #error "RTOS code builds only on ESP32 (not native)."
#endif
#include "rtos/Tasks.h"
#include "rtos/Queues.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "app/RuleEngine.h"
#include "app/Config.h"
#include "app/HardwareConfig.h"

#include "services/CommandDispatcher.h"
#include "services/Logger.h"
#include "services/Notify.h"

#include "actuators/Buzzer.h"
#include "actuators/Servo.h"

#include "sensors/ReedSensor.h"
#include "sensors/PirSensor.h"
#include "sensors/VibrationSensor.h"
#if !KEYPAD_USE_I2C_EXPANDER
#include "drivers/KeypadDriver.h"
#else
#include "drivers/I2CKeypadDriver.h"
#include <Wire.h>
#endif
#include "sensors/KeypadInput.h"

// ถ้าแกทำ ultrasonic + chokepoint แล้ว ค่อยเปิด 2 include นี้
#include "drivers/UltrasonicDriver.h"
#include "sensors/ChokepointSensor.h"

// ===== queues storage =====
namespace RtosQueues {
  QueueHandle_t eventQ = nullptr;
  QueueHandle_t cmdQ   = nullptr;

  bool init() {
    eventQ = xQueueCreate(16, sizeof(Event));
    cmdQ   = xQueueCreate(1, sizeof(CommandMsg)); // latest-only
    return (eventQ != nullptr) && (cmdQ != nullptr);
  }
}

// ===== shared objects =====
static RuleEngine engine;
static Config cfg;

// sensors owned by SensorsTask
static ReedSensor reed1(HwCfg::PIN_REED_1, 1, true, 80);
static PirSensor  pir1(HwCfg::PIN_PIR_1,  1, 1500);
static VibrationSensor vib1(HwCfg::PIN_VIB_1, 1, 600, 700);

#if !KEYPAD_USE_I2C_EXPANDER
static KeypadDriver keypadDrv(HwCfg::KP_ROWS, 4, HwCfg::KP_COLS, 4, HwCfg::KP_MAP, 60);
#else
static I2CKeypadDriver keypadDrv(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60);
#endif
static KeypadInput  keypadIn(0);

// ultrasonic owned by SensorsTask (ถ้าใช้)
static UltrasonicDriver us1(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO);
static ChokepointSensor chokepoint1(&us1, 1); // ถ้า ctor ของแกไม่ตรง เดี๋ยวเราปรับตามไฟล์จริง

// actuators owned by ActuatorsTask
static Buzzer buzzer(HwCfg::PIN_BUZZER, 0);
static Servo  servo1(HwCfg::PIN_SERVO1, 1, 1, 10, 90);
static Servo  servo2(HwCfg::PIN_SERVO2, 2, 2, 10, 90);

static Logger logger;
static Notify notifySvc;
static Actuators acts{ &buzzer, &servo1, &servo2 };

// ===== internal state =====
static TaskHandle_t hSensors = nullptr;
static TaskHandle_t hEngine  = nullptr;
static TaskHandle_t hActs    = nullptr;
static volatile bool g_running = false;

// ===== helpers =====
static inline void pushEventNoWait(const Event& e) {
  if (!RtosQueues::eventQ) return;
  xQueueSend(RtosQueues::eventQ, &e, 0);
}

static void SensorsTask(void*) {
  reed1.begin();
  pir1.begin();
  vib1.begin();

#if KEYPAD_USE_I2C_EXPANDER
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
#endif
  keypadDrv.begin();
  keypadIn.begin();
  keypadIn.setArmCode("1234");
  keypadIn.setDisarmCode("0000");

  // ถ้าใช้ ultrasonic + chokepoint จริง ค่อยเปิด begin
  us1.begin();
  chokepoint1.begin();

  Serial.println("[RTOS] SensorsTask ready");

  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(10);

  for (;;) {
    uint32_t nowMs = millis();

    // keypad -> event
    char k = keypadDrv.update(nowMs);
    if (k) keypadIn.feedKey(k, nowMs);

    Event e;
    if (keypadIn.poll(nowMs, e)) pushEventNoWait(e);

    // sensors priority: reed -> pir -> vib -> chokepoint
    if (reed1.poll(nowMs, e)) pushEventNoWait(e);
    if (pir1.poll(nowMs, e))  pushEventNoWait(e);
    if (vib1.poll(nowMs, e))  pushEventNoWait(e);
    if (chokepoint1.poll(nowMs, e)) pushEventNoWait(e);

    vTaskDelayUntil(&last, period);
  }
}

static void EngineTask(void*) {
  SystemState state; // truth lives here
  Serial.println("[RTOS] EngineTask ready");

  for (;;) {
    Event e;
    if (xQueueReceive(RtosQueues::eventQ, &e, portMAX_DELAY) != pdTRUE) continue;

    Decision d = engine.handle(state, cfg, e);
    state = d.next;

    RtosQueues::CommandMsg msg{ d.cmd, state };
    xQueueOverwrite(RtosQueues::cmdQ, &msg);

    Serial.print("[EV] "); Serial.print((int)e.type);
    Serial.print(" src="); Serial.print((int)e.src);
    Serial.print(" -> [CMD] "); Serial.println((int)d.cmd.type);
  }
}

static void ActuatorsTask(void*) {
  logger.begin();
  notifySvc.begin();

  buzzer.begin();
  servo1.begin();
  servo2.begin();

  Serial.println("[RTOS] ActuatorsTask ready");

  RtosQueues::CommandMsg msg{};
  for (;;) {
    if (xQueueReceive(RtosQueues::cmdQ, &msg, pdMS_TO_TICKS(20)) == pdTRUE) {
      applyCommand(msg.cmd, msg.st, acts, &notifySvc, &logger);
    }

    uint32_t nowMs = millis();
    buzzer.update(nowMs);
    servo1.update(nowMs);
    servo2.update(nowMs);

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

namespace RtosTasks {

void start() {
  if (g_running) return;

  if (!RtosQueues::init()) {
    Serial.println("[RTOS] queue init failed");
    return;
  }

  Serial.println("[RTOS] starting...");

  xTaskCreatePinnedToCore(SensorsTask, "Sensors", 4096, nullptr, 2, &hSensors, 1);
  xTaskCreatePinnedToCore(EngineTask,  "Engine",  4096, nullptr, 3, &hEngine,  1);
  xTaskCreatePinnedToCore(ActuatorsTask,"Acts",   4096, nullptr, 2, &hActs,    0);

  g_running = true;
}

bool running() {
  return g_running;
}

} // namespace
