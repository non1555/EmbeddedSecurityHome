// main.cpp (single-file flattened main_board)
// Generated from src/main_board by flattening headers + sources into one file.
// This is a no-layer-structure reference build draft.

#include <Arduino.h>

// ---- BEGIN FLATTENED CONTENT ----

// ===== FILE: src/main_board\actuators\Buzzer.h =====

class Buzzer {
public:
  Buzzer(uint8_t pin, uint8_t channel = 0);

  void begin();
  void update(uint32_t nowMs);

  void warn();
  void alert();
  void stop();

  bool isActive() const;

private:
  enum class Mode : uint8_t { idle, warn, alert };

  BuzzerDriver drv_;
  Mode mode_;
  uint32_t next_ms_;
  uint8_t step_;
  bool tone_on_;

  void setTone_(bool on, uint32_t hz);
};

// ===== FILE: src/main_board\actuators\Servo.h =====

class Servo {
public:
  Servo(uint8_t pin, uint8_t channel, uint8_t id, uint8_t lock_deg, uint8_t unlock_deg);

  void begin();
  void update(uint32_t nowMs);

  void lock();
  void unlock();

  bool isLocked() const;
  uint8_t id() const;

private:
  ServoDriver drv_;
  uint8_t id_;
  uint8_t lock_deg_;
  uint8_t unlock_deg_;

  uint8_t cur_deg_;
  uint8_t target_deg_;
  uint32_t next_ms_;

  void write_(uint8_t deg);
};

// ===== FILE: src/main_board\app\Commands.h =====

enum class CommandType {
  none,
  buzzer_warn,
  buzzer_alert,
  servo_lock
};

static const char* toString(CommandType t) {
  switch (t) {
    case CommandType::none:         return "none";
    case CommandType::buzzer_warn:  return "buzzer_warn";
    case CommandType::buzzer_alert: return "buzzer_alert";
    case CommandType::servo_lock:   return "servo_lock";
    default:                        return "unknown";
  }
}


struct Command {
  CommandType type;
  uint32_t ts_ms;
};

// ===== FILE: src/main_board\app\Config.h =====

struct Config {
  uint32_t entry_delay_ms = 15000;
  uint32_t exit_grace_after_indoor_activity_ms = 30000;
  uint8_t outdoor_pir_src = 3;
  uint8_t door_ultrasonic_src = 1;
  bool auto_arm_away_on_exit_sequence = true;
  uint32_t auto_arm_exit_sequence_window_ms = 20000;
  bool serial_notify_enabled = false;

  // Door auto-relock session after keypad disarm.
  uint32_t door_unlock_timeout_ms = 15000;
  uint32_t door_unlock_warn_before_ms = 5000;
  uint32_t door_open_hold_warn_after_ms = 10000;
  uint32_t door_warn_retrigger_ms = 350;
};

// ===== FILE: src/main_board\app\DoorUnlockSession.h =====



class DoorUnlockSession {
public:
  void start(uint32_t nowMs, bool doorOpen, const Config& cfg);
  void clear(bool stopBuzzer, Buzzer& buzzer);
  void update(uint32_t nowMs,
              bool doorOpen,
              const Config& cfg,
              Servo& doorServo,
              Buzzer& buzzer,
              bool serialNotifyEnabled);

  bool silenceHoldWarning(bool doorOpen, Buzzer& buzzer, bool serialNotifyEnabled);
  bool countdown(uint32_t nowMs,
                 bool doorLocked,
                 bool doorOpen,
                 const Config& cfg,
                 uint32_t& deadlineMs,
                 uint32_t& warnBeforeMs) const;

  bool isActive() const;

private:
  bool active_ = false;
  bool sawOpen_ = false;
  bool doorWasOpenLastTick_ = false;
  bool holdWarnActive_ = false;
  bool holdWarnSilenced_ = false;
  uint32_t unlockDeadlineMs_ = 0;
  uint32_t openWarnAtMs_ = 0;
  uint32_t closeLockAtMs_ = 0;
  uint32_t nextWarnMs_ = 0;
};


// ===== FILE: src/main_board\app\Events.h =====

enum class EventType {
  disarm,
  arm_away,
  door_open,
  window_open,
  door_tamper,
  vib_spike,
  motion,
  chokepoint,
  door_hold_warn_silence,
  keypad_help_request,
  door_code_unlock,
  door_code_bad,
  manual_door_toggle,
  manual_window_toggle,
  entry_timeout
};

static const char* toString(EventType t) {
  switch (t) {
    case EventType::arm_away:    return "arm_away";
    case EventType::disarm:      return "disarm";
    case EventType::door_open:   return "door_open";
    case EventType::window_open: return "window_open";
    case EventType::door_tamper: return "door_tamper";
    case EventType::vib_spike:   return "vib_spike";
    case EventType::motion:      return "motion";
    case EventType::chokepoint:  return "chokepoint";
    case EventType::door_hold_warn_silence: return "door_hold_warn_silence";
    case EventType::keypad_help_request: return "keypad_help_request";
    case EventType::door_code_unlock: return "door_code_unlock";
    case EventType::door_code_bad: return "door_code_bad";
    case EventType::manual_door_toggle: return "manual_door_toggle";
    case EventType::manual_window_toggle: return "manual_window_toggle";
    case EventType::entry_timeout:return "entry_timeout";
    default:                     return "unknown";
  }
}

struct Event {
  EventType type = EventType::disarm;
  uint32_t ts_ms = 0;
  uint8_t src = 0;

  Event() = default;
  constexpr Event(EventType t, uint32_t ts, uint8_t s = 0)
  : type(t), ts_ms(ts), src(s) {}
};

// Serial synthetic source range for debug/test injection.
constexpr uint8_t kSerialSyntheticSrcBase = 200;
constexpr uint8_t kSerialSyntheticSrcGeneric = 200;
constexpr uint8_t kSerialSyntheticSrcPir1 = 201;
constexpr uint8_t kSerialSyntheticSrcPir2 = 202;
constexpr uint8_t kSerialSyntheticSrcPir3 = 203;
constexpr uint8_t kSerialSyntheticSrcUs1 = 211;
constexpr uint8_t kSerialSyntheticSrcUs2 = 212;
constexpr uint8_t kSerialSyntheticSrcUs3 = 213;

static inline bool isSerialSyntheticSource(uint8_t src) {
  return src >= kSerialSyntheticSrcBase;
}

// ===== FILE: src/main_board\app\HardwareConfig.h =====

namespace HwCfg {

constexpr uint8_t PIN_UNUSED = 255;

// ============================
// Actuators
// ============================
constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_SERVO1 = 26;
constexpr uint8_t PIN_SERVO2 = 27;

// ============================
// Perimeter Contacts (Reed)
// ============================
constexpr uint8_t PIN_REED_1 = 32; // à¸›à¸£à¸°à¸•à¸¹à¸«à¸¥à¸±à¸ (door reed)
constexpr uint8_t PIN_REED_2 = 19; // à¸«à¸™à¹‰à¸²à¸•à¹ˆà¸²à¸‡ (window reed)

// ============================
// PIR
// ============================
constexpr uint8_t PIN_PIR_1  = 35; // à¹‚à¸‹à¸™à¹ƒà¸™à¸šà¹‰à¸²à¸™ 1
constexpr uint8_t PIN_PIR_2  = 36; // à¹‚à¸‹à¸™à¹ƒà¸™à¸šà¹‰à¸²à¸™ 2
constexpr uint8_t PIN_PIR_3  = 39; // à¹‚à¸‹à¸™à¸™à¸­à¸à¸šà¹‰à¸²à¸™ (outdoor PIR)

// ============================
// Vibration
// ============================
constexpr uint8_t PIN_VIB_1  = 34; // vibration à¸£à¸§à¸¡ (à¸›à¸£à¸°à¸•à¸¹/à¸«à¸™à¹‰à¸²à¸•à¹ˆà¸²à¸‡)

// ============================
// Ultrasonic
// ============================
constexpr uint8_t PIN_US_TRIG = 13;   // chokepoint #1: à¹‚à¸‹à¸™à¸›à¸£à¸°à¸•à¸¹
constexpr uint8_t PIN_US_ECHO = 14;   // chokepoint #1: à¹‚à¸‹à¸™à¸›à¸£à¸°à¸•à¸¹
constexpr uint8_t PIN_US_TRIG_2 = 16; // chokepoint #2: à¸—à¸²à¸‡à¸œà¹ˆà¸²à¸™à¸£à¸°à¸«à¸§à¹ˆà¸²à¸‡à¸«à¹‰à¸­à¸‡
constexpr uint8_t PIN_US_ECHO_2 = 17; // chokepoint #2: à¸—à¸²à¸‡à¸œà¹ˆà¸²à¸™à¸£à¸°à¸«à¸§à¹ˆà¸²à¸‡à¸«à¹‰à¸­à¸‡
constexpr uint8_t PIN_US_TRIG_3 = 4;  // chokepoint #3: à¸—à¸²à¸‡à¸œà¹ˆà¸²à¸™à¸£à¸°à¸«à¸§à¹ˆà¸²à¸‡à¸«à¹‰à¸­à¸‡
constexpr uint8_t PIN_US_ECHO_3 = 5;  // chokepoint #3: à¸—à¸²à¸‡à¸œà¹ˆà¸²à¸™à¸£à¸°à¸«à¸§à¹ˆà¸²à¸‡à¸«à¹‰à¸­à¸‡

constexpr uint8_t PIN_BTN_DOOR_TOGGLE = 33;
constexpr uint8_t PIN_BTN_WINDOW_TOGGLE = 18;

// ============================
// I2C Bus (Shared)
// ============================
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t KEYPAD_I2C_ADDR = 0x20;
constexpr uint8_t OLED_I2C_ADDR = 0x3C;
constexpr uint8_t BH1750_I2C_ADDR = 0x23;

constexpr char KP_MAP[16] = {
  '1','2','3','A',
  '4','5','6','B',
  '7','8','9','C',
  '*','0','#','D'
};

// ============================
// Home Automation
// ============================
// Main board no longer owns home-automation IO in active single-board runtime.
constexpr uint8_t PIN_STATUS_LED = PIN_UNUSED;
constexpr uint8_t PIN_RELAY_LIGHT = PIN_UNUSED;

constexpr uint8_t PIN_DHT = PIN_UNUSED;
constexpr uint8_t PIN_FAN = PIN_UNUSED;

constexpr uint8_t PIN_L293D_IN1 = PIN_UNUSED;
constexpr uint8_t PIN_L293D_IN2 = PIN_UNUSED;
constexpr uint8_t FAN_LEDC_CH = 3;
constexpr uint32_t FAN_PWM_HZ = 5000;
constexpr uint8_t FAN_PWM_RES_BITS = 8;
constexpr uint8_t FAN_PWM_DUTY_ON = 200;

constexpr float TEMP_ON_C = 30.0f;
constexpr float TEMP_OFF_C = 27.0f;
constexpr uint32_t TEMP_POLL_MS = 2000;

} // namespace HwCfg

// ===== FILE: src/main_board\app\RuleEngine.h =====

struct Decision {
  SystemState next;
  Command cmd;
};

class RuleEngine {
public:
  Decision handle(const SystemState& s, const Config& cfg, const Event& e) const;
};

// ===== FILE: src/main_board\app\SecurityOrchestrator.h =====

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>


class SecurityOrchestrator {
public:
  void begin();
  void tick(uint32_t nowMs);

private:
  RuleEngine engine_;
  SystemState state_;
  Config cfg_;

  EventCollector collector_;
  MqttBus mqttBus_;

  Buzzer buzzer_{HwCfg::PIN_BUZZER, 0};
  Servo servo1_{HwCfg::PIN_SERVO1, 1, 1, 10, 90};
  Servo servo2_{HwCfg::PIN_SERVO2, 2, 2, 10, 90};
  Logger logger_;
  Actuators acts_{&buzzer_, &servo1_, &servo2_};

  void applyDecision(const Event& e);
  void printEventDecision(const Event& e, const Decision& d, const SystemState& prev) const;
  void processRemoteCommand(const String& payload);
  bool processManualActuatorEvent(const Event& e);
  bool processDoorHoldWarnSilenceEvent(const Event& e);
  bool processKeypadHelpRequestEvent(const Event& e);
  bool processModeEvent(const Event& e, const char* origin);
  void startDoorUnlockSession(uint32_t nowMs);
  void clearDoorUnlockSession(bool stopBuzzer);
  void updateDoorUnlockSession(uint32_t nowMs);
  void syncLiveSnapshot();
  void publishStateStatus(const char* reason);
  void publishStateEvent(const Event& e, const Command& cmd);
  void notify(const String& msg) const;
  void tickUnlocked(uint32_t nowMs);
  void runtimeLoop();
  void statusLoop();
  static void runtimeTaskEntry(void* arg);
  static void tickerTaskEntry(void* arg);
  static void statusTaskEntry(void* arg);

  DoorUnlockSession doorSession_;

  bool servo1WasLocked_ = false;
  bool servo2WasLocked_ = false;

  SemaphoreHandle_t stateMu_ = nullptr;
  SemaphoreHandle_t tickSem_ = nullptr;
  TaskHandle_t runtimeTaskHandle_ = nullptr;
  TaskHandle_t tickerTaskHandle_ = nullptr;
  TaskHandle_t statusTaskHandle_ = nullptr;
  bool tasksStarted_ = false;
};

// ===== FILE: src/main_board\app\SystemState.h =====

enum class Mode { startup_safe, disarm, away };
enum class AlarmLevel { off, warn, alert };

static inline const char* toString(Mode m) {
  switch (m) {
    case Mode::startup_safe: return "startup_safe";
    case Mode::disarm: return "disarm";
    case Mode::away:   return "away";
    default:           return "unknown";
  }
}

static inline const char* toString(AlarmLevel lv) {
  switch (lv) {
    case AlarmLevel::off:      return "off";
    case AlarmLevel::warn:     return "warn";
    case AlarmLevel::alert:    return "alert";
    default:                  return "unknown";
  }
}

struct SystemState {
  Mode mode = Mode::disarm;
  AlarmLevel level = AlarmLevel::off;

  uint32_t last_notify_ms = 0;
  uint32_t last_indoor_activity_ms = 0;
  bool entry_pending = false;
  uint32_t entry_deadline_ms = 0;
  uint32_t last_exit_door_open_ms = 0;

  bool keep_window_locked_when_disarmed = false;
  bool door_locked = false;
  bool window_locked = false;
  bool door_open = false;
  bool window_open = false;
};

// ===== FILE: src/main_board\drivers\BuzzerDriver.h =====

class BuzzerDriver {
public:
  BuzzerDriver(uint8_t pin, uint8_t channel = 0, uint8_t resolution_bits = 10);

  void begin();
  void startTone(uint32_t hz);
  void stopTone();

private:
  uint8_t pin_;
  uint8_t ch_;
  uint8_t res_;
  uint32_t cur_hz_;
};

// ===== FILE: src/main_board\drivers\I2CKeypadDriver.h =====
#include <Wire.h>

class I2CKeypadDriver {
public:
  // PCF8574 mapping:
  // P0..P3 -> rows, P4..P7 -> cols
  I2CKeypadDriver(TwoWire* wire, uint8_t addr7,
                  const char* keymap, uint32_t debounce_ms = 60);

  bool begin();
  char update(uint32_t nowMs);

private:
  TwoWire* wire_;
  uint8_t addr7_;
  const char* keymap_;
  uint32_t debounce_ms_;

  uint8_t scanRow_;
  bool waitingRelease_;
  char lastKey_;
  uint32_t lastKeyMs_;
  uint8_t shadow_;

  bool writePort_(uint8_t value);
  int readColPressed_();
  bool setRowActive_(uint8_t r);
  bool setAllRowsHigh_();
  char mapKey_(uint8_t r, uint8_t c) const;
};

// ===== FILE: src/main_board\drivers\ServoDriver.h =====

class ServoDriver {
public:
  ServoDriver(uint8_t pin, uint8_t channel, uint8_t resolution_bits = 16);

  void begin();
  void writePulseUs(uint16_t us);
  void writeAngle(uint8_t deg);

private:
  uint8_t pin_;
  uint8_t ch_;
  uint8_t res_;
  uint16_t min_us_;
  uint16_t max_us_;

  uint16_t clampUs_(uint16_t us) const;
};

// ===== FILE: src/main_board\drivers\UltrasonicDriver.h =====

class UltrasonicDriver {
public:
  UltrasonicDriver(uint8_t trigPin, uint8_t echoPin);

  void begin();

  // returns distance in cm, or -1 if timeout/no echo
  int readCm(uint32_t timeout_us = 25000);

private:
  uint8_t trig_;
  uint8_t echo_;
};

// ===== FILE: src/main_board\pipelines\EventCollector.h =====



#include <Wire.h>

class EventCollector {
public:
  EventCollector();

  void begin();
  bool pollKeypad(uint32_t nowMs, Event& out);
  bool pollSensorOrSerial(uint32_t nowMs, Event& out);
  void printSerialHelp() const;
  bool isDoorOpen() const;
  bool isWindowOpen() const;
  void updateOledStatus(uint32_t nowMs,
                        bool doorLocked,
                        bool doorOpen,
                        bool countdownActive,
                        uint32_t countdownDeadlineMs,
                        uint32_t countdownWarnBeforeMs);

private:
  UltrasonicDriver us1_;
  ChokepointSensor chokep1_;
  UltrasonicDriver us2_;
  ChokepointSensor chokep2_;
  UltrasonicDriver us3_;
  ChokepointSensor chokep3_;

  ReedSensor reedDoor_;
  ReedSensor reedWindow_;
  PirSensor pir1_;
  PirSensor pir2_;
  PirSensor pir3_;

  // Multiple vibration switches wired together into one input.
  VibrationSensor vibCombined_;

  OledCodeUi oled_{HwCfg::OLED_I2C_ADDR};

  I2CKeypadDriver keypadDrv_;
  KeypadInput keypadIn_;

  bool pollManualButton(uint8_t pin,
                        uint32_t nowMs,
                        uint32_t debounceMs,
                        bool& lastRawPressed,
                        bool& stablePressed,
                        uint32_t& lastChangeMs,
                        EventType pressEvent,
                        Event& out);
  bool pollManualButtons(uint32_t nowMs, Event& out);
  bool parseSerialEvent(char c, uint32_t nowMs, Event& out) const;
  bool parseSerialEvent(const String& token, uint32_t nowMs, Event& out) const;
  bool parseSerialCode(uint16_t code, uint32_t nowMs, Event& out) const;
  bool readSerialEvent(uint32_t nowMs, Event& out);

  bool doorToggleLastRawPressed_ = false;
  bool doorToggleStablePressed_ = false;
  uint32_t doorToggleLastChangeMs_ = 0;

  bool windowToggleLastRawPressed_ = false;
  bool windowToggleStablePressed_ = false;
  uint32_t windowToggleLastChangeMs_ = 0;

  char serialLineBuf_[48] = {0};
  uint8_t serialLineLen_ = 0;
  uint32_t serialLineLastByteMs_ = 0;
};

// ===== FILE: src/main_board\rtos\Queues.h =====



#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace RtosQueues {

enum class PublishKind : uint8_t {
  event,
  status,
  ack
};

struct PublishMsg {
  PublishKind kind = PublishKind::event;
  Event e{};
  SystemState st{};
  Command cmd{CommandType::none, 0};
  bool ok = false;
  char text1[32]{};
  char text2[32]{};
};

struct CmdMsg {
  char payload[128]{};
};

extern QueueHandle_t mqttPubQ;
extern QueueHandle_t mqttCmdQ;

bool init();

} // namespace RtosQueues

// ===== FILE: src/main_board\rtos\Tasks.h =====



namespace RtosTasks {

void attachMqtt(MqttClient* client);
void startIfReady();
bool mqttWorkerStarted();

bool enqueuePublish(const RtosQueues::PublishMsg& msg);
bool dequeueCommand(RtosQueues::CmdMsg& out);

} // namespace RtosTasks

// ===== FILE: src/main_board\sensors\ChokepointSensor.h =====

class ChokepointSensor {
public:
  ChokepointSensor(UltrasonicDriver* drv, uint8_t id,
                   int near_cm = 35,
                   int far_cm = 55,
                   uint32_t sample_period_ms = 200,
                   uint32_t cooldown_ms = 1500);

  void begin();
  bool poll(uint32_t nowMs, Event& out);

  int lastCm() const;
  bool isOffline(uint32_t nowMs, uint32_t noValidMs, uint16_t noEchoCount) const;
  uint16_t consecutiveNoEcho() const;
  uint32_t lastValidMs() const;

private:
  UltrasonicDriver* drv_;
  uint8_t id_;

  int near_cm_;
  int far_cm_;
  uint32_t sample_period_ms_;
  uint32_t cooldown_ms_;

  uint32_t next_sample_ms_;
  uint32_t last_fire_ms_;

  int last_cm_;
  bool inside_;
  uint16_t consecutive_no_echo_;
  uint32_t last_valid_ms_;
  bool seen_valid_once_;
};

// ===== FILE: src/main_board\sensors\KeypadInput.h =====

class KeypadInput {
public:
  KeypadInput(uint8_t id);

  void begin();

  void setDoorCode(const char* code4);

  // feed a raw key from keypad driver
  void feedKey(char k, uint32_t nowMs);

  // returns true if an Event is produced
  bool poll(uint32_t nowMs, Event& out);

  const char* buf() const { return buf_; }
  uint8_t len() const { return len_; }

  enum class SubmitResult : uint8_t { none, ok, bad };
  bool takeSubmitResult(SubmitResult& out);

private:
  uint8_t id_;

  char doorCode_[5];

  char buf_[5];
  uint8_t len_;

  bool hasEvent_;
  Event pending_;

  uint32_t lastKeyMs_;
  SubmitResult lastSubmit_;

  void clear_();
  bool match_(const char* a, const char* b) const;
};

// ===== FILE: src/main_board\sensors\PirSensor.h =====

class PirSensor {
public:
  PirSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms = 1500);

  void begin();
  bool poll(uint32_t nowMs, Event& out);
  bool isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const;

private:
  uint8_t pin_;
  uint8_t id_;
  uint32_t cooldown_ms_;
  uint32_t last_fire_ms_;
  bool last_active_;
  uint32_t active_since_ms_ = 0;
  bool seen_inactive_since_begin_ = false;
};

// ===== FILE: src/main_board\sensors\ReedSensor.h =====

class ReedSensor {
public:
  ReedSensor(
    uint8_t pin,
    uint8_t id,
    EventType open_event = EventType::window_open,
    bool open_is_high = true,
    uint32_t debounce_ms = 80
  );

  void begin();
  bool poll(uint32_t nowMs, Event& out);

  bool isOpen() const;

private:
  uint8_t pin_;
  uint8_t id_;
  EventType open_event_;
  bool open_is_high_;
  uint32_t debounce_ms_;

  bool stable_open_;
  bool last_raw_;
  uint32_t last_flip_ms_;
  bool fired_open_;

  bool readOpenRaw_() const;
};

// ===== FILE: src/main_board\sensors\VibrationSensor.h =====

class VibrationSensor {
public:
  // Vibration switch wired to GND (NC) with pull-up on the input.
  // A "spike" is detected on a LOW->HIGH transition (brief open circuit) with cooldown.
  VibrationSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms = 500);

  void begin();
  bool poll(uint32_t nowMs, Event& out);
  bool isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const;

private:
  uint8_t pin_;
  uint8_t id_;
  uint32_t cooldown_ms_;
  uint32_t last_fire_ms_;
  bool last_active_;
  uint32_t active_since_ms_ = 0;
  bool seen_inactive_since_begin_ = false;
};

// ===== FILE: src/main_board\services\CommandDispatcher.h =====



struct Actuators {
  Buzzer* buzzer = nullptr;
  Servo* servo1 = nullptr;
  Servo* servo2 = nullptr;

  Actuators() = default;
  constexpr Actuators(Buzzer* b, Servo* s1, Servo* s2)
  : buzzer(b), servo1(s1), servo2(s2) {}
};

void applyCommand(const Command& cmd, const SystemState& st, Actuators& acts, Logger* logger = nullptr);

// ===== FILE: src/main_board\services\Logger.h =====

class Logger {
public:
  void begin();

  void logCommand(const Command& cmd, const SystemState& st);
};

// ===== FILE: src/main_board\services\MqttBus.h =====



class MqttBus {
public:
  void begin();

  void publishEvent(const Event& e, const SystemState& st, const Command& cmd);
  void publishStatus(const SystemState& st, const char* reason);
  void publishAck(const char* cmd, bool ok, const char* detail);

  bool pollCommand(String& outPayload);

private:
  bool rtosActive_ = false;
};

// ===== FILE: src/main_board\services\MqttClient.h =====

#include <WiFi.h>
#include <PubSubClient.h>


class MqttClient {
public:
  using CommandCallback = void (*)(const String& topic, const String& payload);

  MqttClient();

  void begin(CommandCallback cb = nullptr);
  void update(uint32_t nowMs);

  bool ready();
  bool publishEvent(const Event& e, const SystemState& st, const Command& cmd);
  bool publishStatus(const SystemState& st, const char* reason);
  bool publishAck(const char* cmd, bool ok, const char* detail);
  bool publishMetrics(
    uint32_t usDrops,
    uint32_t pubDrops,
    uint32_t cmdDrops,
    uint32_t storeDrops,
    uint32_t usQueueDepth,
    uint32_t pubQueueDepth,
    uint32_t cmdQueueDepth,
    uint32_t storeDepth
  );

private:
  static MqttClient* self_;

  WiFiClient wifiClient_;
  PubSubClient mqtt_;
  CommandCallback cmdCb_ = nullptr;

  uint32_t nextWifiRetryMs_ = 0;
  uint32_t nextMqttRetryMs_ = 0;

  static void onMqttMessage(char* topic, uint8_t* payload, unsigned int length);
  void connectWifi(uint32_t nowMs);
  void connectMqtt(uint32_t nowMs);
};

// ===== FILE: src/main_board\ui\OledCodeUi.h =====


class Adafruit_SSD1306;

class OledCodeUi {
public:
  OledCodeUi(uint8_t addr7, uint8_t w = 128, uint8_t h = 64);

  bool begin();
  void showCode(const char* code, uint8_t len);
  void showResult(bool ok);
  // Updates the door status line (does not change keypad/PIN UX).
  void setDoorStatus(bool doorLocked,
                     bool doorOpen,
                     bool countdownActive,
                     uint32_t countdownDeadlineMs,
                     uint32_t countdownWarnBeforeMs);
  void update(uint32_t nowMs);

private:
  uint8_t addr7_;
  uint8_t w_;
  uint8_t h_;

  // We allocate the display object at runtime to keep includes local to .cpp
  // (reduces compile impact on non-UI files).
  Adafruit_SSD1306* disp_;

  char code_[5];
  uint8_t len_;

  bool showing_result_;
  bool last_ok_;
  uint32_t result_until_ms_;

  // Door status line
  bool doorLocked_ = false;
  bool doorOpen_ = false;

  bool countdownActive_ = false;
  uint32_t countdownDeadlineMs_ = 0;
  uint32_t countdownWarnBeforeMs_ = 0;
  int lastCountdownSec_ = -1;
  bool lastCountdownUrgent_ = false;

  bool dirty_ = true;

  void render_();
};

// ===== FILE: src/main_board\actuators\Buzzer.cpp =====

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}
} // namespace

Buzzer::Buzzer(uint8_t pin, uint8_t channel)
: drv_(pin, channel), mode_(Mode::idle), next_ms_(0), step_(0), tone_on_(false) {}

void Buzzer::begin() {
  drv_.begin();
  stop();
}

void Buzzer::setTone_(bool on, uint32_t hz) {
  if (on) drv_.startTone(hz);
  else drv_.stopTone();
  tone_on_ = on;
}

void Buzzer::warn() {
  mode_ = Mode::warn;
  step_ = 0;
  next_ms_ = 0;
  tone_on_ = false;
}

void Buzzer::alert() {
  mode_ = Mode::alert;
  step_ = 0;
  next_ms_ = 0;
  tone_on_ = false;
}

void Buzzer::stop() {
  mode_ = Mode::idle;
  step_ = 0;
  next_ms_ = 0;
  setTone_(false, 0);
}

bool Buzzer::isActive() const {
  return mode_ != Mode::idle;
}

void Buzzer::update(uint32_t nowMs) {
  if (mode_ == Mode::idle) return;
  if (next_ms_ != 0 && !reached(nowMs, next_ms_)) return;

  if (mode_ == Mode::warn) {
    const uint32_t hz = 2200;
    if (!tone_on_) {
      setTone_(true, hz);
      next_ms_ = nowMs + 180;
    } else {
      setTone_(false, 0);
      next_ms_ = nowMs + 220;
      step_++;
      if (step_ >= 6) stop();
    }
    return;
  }

  if (mode_ == Mode::alert) {
    const uint32_t hz = 3200;
    if (!tone_on_) {
      setTone_(true, hz);
      next_ms_ = nowMs + 200;
    } else {
      setTone_(false, 0);
      next_ms_ = nowMs + 120;
    }
    return;
  }
}

// ===== FILE: src/main_board\actuators\Servo.cpp =====

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}
} // namespace

Servo::Servo(uint8_t pin, uint8_t channel, uint8_t id, uint8_t lock_deg, uint8_t unlock_deg)
: drv_(pin, channel),
  id_(id),
  lock_deg_(lock_deg),
  unlock_deg_(unlock_deg),
  cur_deg_(unlock_deg),
  target_deg_(unlock_deg),
  next_ms_(0) {}

void Servo::write_(uint8_t deg) {
  drv_.writeAngle(deg);
  cur_deg_ = deg;
}

void Servo::begin() {
  drv_.begin();
  write_(unlock_deg_);
  target_deg_ = unlock_deg_;
  next_ms_ = 0;
}

void Servo::lock() {
  target_deg_ = lock_deg_;
  next_ms_ = 0;
}

void Servo::unlock() {
  target_deg_ = unlock_deg_;
  next_ms_ = 0;
}

bool Servo::isLocked() const {
  return cur_deg_ == lock_deg_;
}

uint8_t Servo::id() const {
  return id_;
}

void Servo::update(uint32_t nowMs) {
  if (cur_deg_ == target_deg_) return;
  if (next_ms_ != 0 && !reached(nowMs, next_ms_)) return;

  if (cur_deg_ < target_deg_) {
    cur_deg_++;
  } else {
    cur_deg_--;
  }

  drv_.writeAngle(cur_deg_);
  next_ms_ = nowMs + 15;
}

// ===== FILE: src/main_board\app\DoorUnlockSession.cpp =====

namespace {
inline bool reached(uint32_t nowMs, uint32_t deadlineMs) {
  return (int32_t)(nowMs - deadlineMs) >= 0;
}

inline bool before(uint32_t nowMs, uint32_t deadlineMs) {
  return (int32_t)(nowMs - deadlineMs) < 0;
}

inline void emitNotify(bool enabled, const char* msg) {
  (void)enabled;
  (void)msg;
}
} // namespace

void DoorUnlockSession::start(uint32_t nowMs, bool doorOpen, const Config& cfg) {
  active_ = true;
  sawOpen_ = doorOpen;
  holdWarnActive_ = false;
  holdWarnSilenced_ = false;
  unlockDeadlineMs_ = nowMs + cfg.door_unlock_timeout_ms;
  doorWasOpenLastTick_ = doorOpen;
  openWarnAtMs_ = doorOpen ? (nowMs + cfg.door_open_hold_warn_after_ms) : 0;
  closeLockAtMs_ = 0;
  nextWarnMs_ = 0;
}

void DoorUnlockSession::clear(bool stopBuzzer, Buzzer& buzzer) {
  active_ = false;
  sawOpen_ = false;
  holdWarnActive_ = false;
  holdWarnSilenced_ = false;
  unlockDeadlineMs_ = 0;
  openWarnAtMs_ = 0;
  closeLockAtMs_ = 0;
  nextWarnMs_ = 0;
  if (stopBuzzer) buzzer.stop();
}

void DoorUnlockSession::update(uint32_t nowMs,
                               bool doorOpen,
                               const Config& cfg,
                               Servo& doorServo,
                               Buzzer& buzzer,
                               bool serialNotifyEnabled) {
  if (!active_) return;

  static constexpr uint32_t kAutoLockAfterCloseMs = 3000;

  if (!doorWasOpenLastTick_ && doorOpen) {
    sawOpen_ = true;
    holdWarnActive_ = false;
    holdWarnSilenced_ = false;
    openWarnAtMs_ = nowMs + cfg.door_open_hold_warn_after_ms;
    closeLockAtMs_ = 0;
    nextWarnMs_ = 0;
  }

  if (doorWasOpenLastTick_ && !doorOpen) {
    holdWarnActive_ = false;
    holdWarnSilenced_ = false;
    openWarnAtMs_ = 0;
    closeLockAtMs_ = nowMs + kAutoLockAfterCloseMs;
    nextWarnMs_ = 0;
  }
  doorWasOpenLastTick_ = doorOpen;

  if (closeLockAtMs_ != 0) {
    if (doorOpen) {
      closeLockAtMs_ = 0;
    } else if (reached(nowMs, closeLockAtMs_)) {
      doorServo.lock();
      emitNotify(serialNotifyEnabled, "door auto-locked after close");
      clear(true, buzzer);
    }
    return;
  }

  if (!sawOpen_) {
    if (reached(nowMs, unlockDeadlineMs_)) {
      doorServo.lock();
      emitNotify(serialNotifyEnabled, "door auto-locked: unlock timeout");
      clear(true, buzzer);
      return;
    }

    const uint32_t timeLeftMs = unlockDeadlineMs_ - nowMs;
    if (timeLeftMs <= cfg.door_unlock_warn_before_ms &&
        (nextWarnMs_ == 0 || reached(nowMs, nextWarnMs_))) {
      buzzer.warn();
      nextWarnMs_ = nowMs + cfg.door_warn_retrigger_ms;
    }
    return;
  }

  if (doorOpen && openWarnAtMs_ != 0 && reached(nowMs, openWarnAtMs_)) {
    holdWarnActive_ = true;
    if (!holdWarnSilenced_ && (nextWarnMs_ == 0 || reached(nowMs, nextWarnMs_))) {
      buzzer.warn();
      nextWarnMs_ = nowMs + cfg.door_warn_retrigger_ms;
    }
  }
}

bool DoorUnlockSession::silenceHoldWarning(bool doorOpen, Buzzer& buzzer, bool serialNotifyEnabled) {
  if (!(active_ && doorOpen && holdWarnActive_)) return false;
  holdWarnSilenced_ = true;
  buzzer.stop();
  emitNotify(serialNotifyEnabled, "door-open warning silenced");
  return true;
}

bool DoorUnlockSession::countdown(uint32_t nowMs,
                                  bool doorLocked,
                                  bool doorOpen,
                                  const Config& cfg,
                                  uint32_t& deadlineMs,
                                  uint32_t& warnBeforeMs) const {
  deadlineMs = 0;
  warnBeforeMs = 0;
  if (!active_ || doorLocked) return false;

  if (!sawOpen_) {
    deadlineMs = unlockDeadlineMs_;
    warnBeforeMs = cfg.door_unlock_warn_before_ms;
    return (deadlineMs != 0 && before(nowMs, deadlineMs));
  }
  if (doorOpen) {
    deadlineMs = openWarnAtMs_;
    warnBeforeMs = 2000;
    return (deadlineMs != 0 && before(nowMs, deadlineMs));
  }
  if (closeLockAtMs_ != 0) {
    deadlineMs = closeLockAtMs_;
    warnBeforeMs = 1000;
    return before(nowMs, deadlineMs);
  }
  return false;
}

bool DoorUnlockSession::isActive() const {
  return active_;
}

// ===== FILE: src/main_board\app\RuleEngine.cpp =====

namespace {
bool within(uint32_t nowMs, uint32_t refMs, uint32_t windowMs) {
  return refMs != 0 && (nowMs - refMs) <= windowMs;
}

bool isOutdoorMotion(const Event& e, const Config& cfg) {
  if (e.type != EventType::motion) return false;
  uint8_t src = e.src;
  if (src == kSerialSyntheticSrcPir1) src = 1;
  else if (src == kSerialSyntheticSrcPir2) src = 2;
  else if (src == kSerialSyntheticSrcPir3) src = 3;
  return src == cfg.outdoor_pir_src;
}

bool isDoorZoneChokepoint(const Event& e, const Config& cfg) {
  return e.type == EventType::chokepoint && e.src == cfg.door_ultrasonic_src;
}

bool isIndoorActivity(const Event& e, const Config& cfg) {
  if (e.type == EventType::chokepoint) return true;
  if (e.type != EventType::motion) return false;
  return !isOutdoorMotion(e, cfg);
}

AlarmLevel stepUp(AlarmLevel lv) {
  if (lv == AlarmLevel::off) return AlarmLevel::warn;
  if (lv == AlarmLevel::warn) return AlarmLevel::alert;
  return AlarmLevel::alert;
}

void resetForMode(SystemState& st, Mode mode) {
  st.mode = mode;
  st.level = AlarmLevel::off;
  st.entry_pending = false;
  st.entry_deadline_ms = 0;
  st.last_exit_door_open_ms = 0;
  st.keep_window_locked_when_disarmed = false;
}
} // namespace

Decision RuleEngine::handle(const SystemState& s, const Config& cfg, const Event& e) const {
  Decision d{ s, {CommandType::none, e.ts_ms} };

  if (e.type == EventType::disarm) {
    resetForMode(d.next, Mode::disarm);
    return d;
  }

  if (e.type == EventType::arm_away) {
    resetForMode(d.next, Mode::away);
    return d;
  }

  if (s.mode == Mode::disarm && cfg.auto_arm_away_on_exit_sequence) {
    if (e.type == EventType::door_open) {
      d.next.last_exit_door_open_ms = e.ts_ms;
      return d;
    }

    if ((isOutdoorMotion(e, cfg) || isDoorZoneChokepoint(e, cfg)) &&
        within(e.ts_ms, s.last_exit_door_open_ms, cfg.auto_arm_exit_sequence_window_ms)) {
      resetForMode(d.next, Mode::away);
    }
    return d;
  }

  if (s.mode != Mode::away) {
    return d;
  }

  if (isIndoorActivity(e, cfg)) {
    d.next.last_indoor_activity_ms = e.ts_ms;
  }

  if (e.type == EventType::door_open && s.door_locked) {
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.level = AlarmLevel::alert;
    d.cmd.type = CommandType::buzzer_alert;
    return d;
  }

  if (e.type == EventType::door_open) {
    if (s.entry_pending) return d;
    if (s.last_indoor_activity_ms != 0 &&
        (e.ts_ms - s.last_indoor_activity_ms) <= cfg.exit_grace_after_indoor_activity_ms) {
      return d;
    }
    d.next.entry_pending = true;
    d.next.entry_deadline_ms = e.ts_ms + cfg.entry_delay_ms;
    d.next.level = AlarmLevel::warn;
    d.cmd.type = CommandType::buzzer_warn;
    return d;
  }

  if (e.type == EventType::entry_timeout) {
    if (!s.entry_pending) return d;
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.level = AlarmLevel::alert;
    d.cmd.type = CommandType::buzzer_alert;
    return d;
  }

  if (e.type == EventType::door_tamper) {
    d.next.entry_pending = false;
    d.next.entry_deadline_ms = 0;
    d.next.level = AlarmLevel::alert;
    d.cmd.type = CommandType::buzzer_alert;
    return d;
  }

  if (e.type == EventType::window_open ||
      e.type == EventType::vib_spike ||
      isIndoorActivity(e, cfg)) {
    d.next.level = stepUp(s.level);
    d.cmd.type = (d.next.level == AlarmLevel::alert) ? CommandType::buzzer_alert
                                                      : CommandType::buzzer_warn;
    return d;
  }

  return d;
}

// ===== FILE: src/main_board\app\SecurityOrchestrator.cpp =====

namespace {
String normalize(String s) {
  s.trim();
  s.toLowerCase();
  return s;
}

void emitSerialNotify(bool enabled, const String& msg) {
  (void)enabled;
  (void)msg;
}

bool isModeEvent(EventType t) {
  return t == EventType::disarm || t == EventType::arm_away;
}

bool isArmedMode(Mode mode) {
  return mode == Mode::away;
}

bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

constexpr uint32_t STATUS_HEARTBEAT_MS = 5000;
constexpr uint32_t ORCH_TICK_PERIOD_MS = 20;
} // namespace

void SecurityOrchestrator::printEventDecision(const Event& e,
                                              const Decision& d,
                                              const SystemState& prev) const {
  (void)e;
  (void)d;
  (void)prev;
}

void SecurityOrchestrator::syncLiveSnapshot() {
  state_.door_locked = servo1_.isLocked();
  state_.window_locked = servo2_.isLocked();
  state_.door_open = collector_.isDoorOpen();
  state_.window_open = collector_.isWindowOpen();
}

void SecurityOrchestrator::publishStateStatus(const char* reason) {
  syncLiveSnapshot();
  mqttBus_.publishStatus(state_, reason);
}

void SecurityOrchestrator::publishStateEvent(const Event& e, const Command& cmd) {
  syncLiveSnapshot();
  mqttBus_.publishEvent(e, state_, cmd);
}

void SecurityOrchestrator::notify(const String& msg) const {
  emitSerialNotify(cfg_.serial_notify_enabled, msg);
}

void SecurityOrchestrator::applyDecision(const Event& e) {
  syncLiveSnapshot();
  const SystemState prev = state_;
  const Decision d = engine_.handle(state_, cfg_, e);
  state_ = d.next;
  applyCommand(d.cmd, state_, acts_, &logger_);
  if (isArmedMode(state_.mode)) clearDoorUnlockSession(true);
  publishStateEvent(e, d.cmd);
  publishStateStatus(toString(e.type));
  printEventDecision(e, d, prev);
}

void SecurityOrchestrator::startDoorUnlockSession(uint32_t nowMs) {
  doorSession_.start(nowMs, collector_.isDoorOpen(), cfg_);
}

void SecurityOrchestrator::clearDoorUnlockSession(bool stopBuzzer) {
  doorSession_.clear(stopBuzzer, buzzer_);
}

void SecurityOrchestrator::updateDoorUnlockSession(uint32_t nowMs) {
  doorSession_.update(nowMs,
                      collector_.isDoorOpen(),
                      cfg_,
                      servo1_,
                      buzzer_,
                      cfg_.serial_notify_enabled);
}

void SecurityOrchestrator::begin() {
  logger_.begin();
  collector_.begin();
  mqttBus_.begin();

  buzzer_.begin();
  servo1_.begin();
  servo2_.begin();

  if (!collector_.isDoorOpen()) servo1_.lock();
  if (!collector_.isWindowOpen()) servo2_.lock();

  servo1WasLocked_ = servo1_.isLocked();
  servo2WasLocked_ = servo2_.isLocked();

  publishStateStatus("boot");

  stateMu_ = xSemaphoreCreateMutex();
  tickSem_ = xSemaphoreCreateBinary();

  xTaskCreatePinnedToCore(runtimeTaskEntry,
                          "sec_runtime",
                          6144,
                          this,
                          2,
                          &runtimeTaskHandle_,
                          1);
  xTaskCreatePinnedToCore(tickerTaskEntry,
                          "sec_tick",
                          2048,
                          this,
                          2,
                          &tickerTaskHandle_,
                          1);
  xTaskCreatePinnedToCore(statusTaskEntry,
                          "sec_status",
                          3072,
                          this,
                          1,
                          &statusTaskHandle_,
                          1);

  tasksStarted_ = true;
  if (tickSem_) xSemaphoreGive(tickSem_);
}

bool SecurityOrchestrator::processModeEvent(const Event& e, const char* origin) {
  if (!isModeEvent(e.type)) return false;
  applyDecision(e);
  (void)origin;
  return true;
}

void SecurityOrchestrator::processRemoteCommand(const String& payload) {
  const String cmd = normalize(payload);
  if (cmd.length() == 0) return;

  const uint32_t nowMs = millis();

  if (cmd == "status") {
    String msg = "mode=" + String((int)state_.mode) +
                 " level=" + String((int)state_.level) +
                 " door_locked=" + String(servo1_.isLocked() ? "1" : "0") +
                 " window_locked=" + String(servo2_.isLocked() ? "1" : "0") +
                 " door_open=" + String(collector_.isDoorOpen() ? "1" : "0") +
                 " window_open=" + String(collector_.isWindowOpen() ? "1" : "0");
    notify(msg);
    mqttBus_.publishAck("status", true, "ok");
    publishStateStatus("remote_status");
    return;
  }

  if (cmd == "buzz" || cmd == "buzzer" || cmd == "buzz warn" || cmd == "buzzer warn") {
    buzzer_.warn();
    mqttBus_.publishAck("buzz", true, "ok");
    publishStateStatus("remote_buzz");
    return;
  }

  if (cmd == "alarm" || cmd == "alarm on" || cmd == "buzz alarm" || cmd == "buzzer alert") {
    buzzer_.alert();
    mqttBus_.publishAck("alarm", true, "ok");
    publishStateStatus("remote_alarm");
    return;
  }

  if (cmd == "silence" || cmd == "alarm off" || cmd == "buzz stop" || cmd == "buzzer stop") {
    buzzer_.stop();
    mqttBus_.publishAck("silence", true, "ok");
    publishStateStatus("remote_silence");
    return;
  }

  if (cmd == "disarm" || cmd == "mode disarm") {
    processModeEvent({EventType::disarm, nowMs, 9}, "REMOTE");
    mqttBus_.publishAck("disarm", true, "ok");
    return;
  }

  if (cmd == "arm away" || cmd == "arm_away" || cmd == "mode away") {
    processModeEvent({EventType::arm_away, nowMs, 9}, "REMOTE");
    mqttBus_.publishAck("arm away", true, "ok");
    return;
  }

  if (cmd == "lock door") {
    servo1_.lock();
    clearDoorUnlockSession(true);
    mqttBus_.publishAck("lock door", true, "ok");
    publishStateStatus("remote_lock_door");
    return;
  }

  if (cmd == "lock window") {
    state_.keep_window_locked_when_disarmed = true;
    servo2_.lock();
    mqttBus_.publishAck("lock window", true, "ok");
    publishStateStatus("remote_lock_window");
    return;
  }

  if (cmd == "lock all") {
    servo1_.lock();
    clearDoorUnlockSession(true);
    state_.keep_window_locked_when_disarmed = true;
    servo2_.lock();
    mqttBus_.publishAck("lock all", true, "ok");
    publishStateStatus("remote_lock_all");
    return;
  }

  if (cmd == "unlock door") {
    servo1_.unlock();
    clearDoorUnlockSession(true);
    startDoorUnlockSession(nowMs);
    mqttBus_.publishAck("unlock door", true, "ok");
    publishStateStatus("remote_unlock_door");
    return;
  }

  if (cmd == "unlock window") {
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    mqttBus_.publishAck("unlock window", true, "ok");
    publishStateStatus("remote_unlock_window");
    return;
  }

  if (cmd == "unlock all") {
    servo1_.unlock();
    clearDoorUnlockSession(true);
    startDoorUnlockSession(nowMs);
    state_.keep_window_locked_when_disarmed = false;
    servo2_.unlock();
    mqttBus_.publishAck("unlock all", true, "ok");
    publishStateStatus("remote_unlock_all");
    return;
  }
}

bool SecurityOrchestrator::processDoorHoldWarnSilenceEvent(const Event& e) {
  if (e.type != EventType::door_hold_warn_silence) return false;
  doorSession_.silenceHoldWarning(collector_.isDoorOpen(), buzzer_, cfg_.serial_notify_enabled);
  return true;
}

bool SecurityOrchestrator::processKeypadHelpRequestEvent(const Event& e) {
  if (e.type != EventType::keypad_help_request) return false;

  notify("HELP requested from keypad");
  publishStateEvent(e, {CommandType::none, e.ts_ms});
  publishStateStatus("keypad_help_request");
  return true;
}

bool SecurityOrchestrator::processManualActuatorEvent(const Event& e) {
  auto publishManual = [&](const char* reason) {
    publishStateEvent(e, {CommandType::none, e.ts_ms});
    publishStateStatus(reason);
  };

  if (e.type == EventType::manual_door_toggle) {
    if (servo1_.isLocked()) {
      servo1_.unlock();
      clearDoorUnlockSession(true);
      startDoorUnlockSession(e.ts_ms);
      notify("manual door: unlocked");
      publishManual("manual_door_unlock");
      return true;
    }

    servo1_.lock();
    clearDoorUnlockSession(true);
    notify("manual door: locked");
    publishManual("manual_door_lock");
    return true;
  }

  if (e.type == EventType::manual_window_toggle) {
    if (servo2_.isLocked()) {
      state_.keep_window_locked_when_disarmed = false;
      servo2_.unlock();
      notify("manual window: unlocked");
      publishManual("manual_window_unlock");
      return true;
    }

    state_.keep_window_locked_when_disarmed = true;
    servo2_.lock();
    notify("manual window: locked");
    publishManual("manual_window_lock");
    return true;
  }

  return false;
}

void SecurityOrchestrator::tick(uint32_t nowMs) {
  if (tasksStarted_) {
    vTaskDelay(pdMS_TO_TICKS(50));
    return;
  }

  if (stateMu_) xSemaphoreTake(stateMu_, portMAX_DELAY);
  tickUnlocked(nowMs);
  if (stateMu_) xSemaphoreGive(stateMu_);
}

void SecurityOrchestrator::tickUnlocked(uint32_t nowMs) {
  Event e;

  buzzer_.update(nowMs);
  servo1_.update(nowMs);
  servo2_.update(nowMs);

  const bool prevDoorLocked = servo1WasLocked_;
  const bool prevWindowLocked = servo2WasLocked_;
  const bool doorLockedNow = servo1_.isLocked();
  const bool windowLockedNow = servo2_.isLocked();

  if (!doorSession_.isActive() && prevDoorLocked && !doorLockedNow && !collector_.isDoorOpen()) {
    startDoorUnlockSession(nowMs);
  }

  servo1WasLocked_ = doorLockedNow;
  servo2WasLocked_ = windowLockedNow;

  if ((prevDoorLocked != doorLockedNow) || (prevWindowLocked != windowLockedNow)) {
    publishStateStatus("actuator_lock_state_changed");
  }

  updateDoorUnlockSession(nowMs);

  bool cdActive = false;
  uint32_t cdDeadline = 0;
  uint32_t cdWarnBefore = 0;
  cdActive = doorSession_.countdown(nowMs,
                                    servo1_.isLocked(),
                                    collector_.isDoorOpen(),
                                    cfg_,
                                    cdDeadline,
                                    cdWarnBefore);
  collector_.updateOledStatus(nowMs,
                              servo1_.isLocked(),
                              collector_.isDoorOpen(),
                              cdActive,
                              cdDeadline,
                              cdWarnBefore);

  String remoteCmd;
  if (mqttBus_.pollCommand(remoteCmd)) {
    processRemoteCommand(remoteCmd);
  }

  if (collector_.pollKeypad(nowMs, e)) {
    if (processDoorHoldWarnSilenceEvent(e)) return;
    if (processKeypadHelpRequestEvent(e)) return;

    if (e.type == EventType::door_code_unlock) {
      if (state_.mode != Mode::disarm) {
        processModeEvent({EventType::disarm, nowMs, e.src}, "KEYPAD");
      }
      servo1_.unlock();
      state_.keep_window_locked_when_disarmed = true;
      servo2_.lock();
      clearDoorUnlockSession(true);
      startDoorUnlockSession(nowMs);
      notify("door code accepted");
      return;
    }

    if (e.type == EventType::door_code_bad) {
      notify("wrong door code");
      mqttBus_.publishAck("door_code", true, "wrong");
      publishStateStatus("door_code_bad");
      return;
    }

    if (processManualActuatorEvent(e)) return;
    if (isModeEvent(e.type)) {
      processModeEvent(e, "KEYPAD");
      return;
    }
  }

  if (state_.entry_pending && reached(nowMs, state_.entry_deadline_ms)) {
    e = {EventType::entry_timeout, nowMs, 0};
    applyDecision(e);
    return;
  }

  if (!collector_.pollSensorOrSerial(nowMs, e)) return;

  if (processDoorHoldWarnSilenceEvent(e)) return;
  if (processKeypadHelpRequestEvent(e)) return;
  if (processManualActuatorEvent(e)) return;
  if (isModeEvent(e.type)) {
    processModeEvent(e, "INPUT");
    return;
  }

  applyDecision(e);
}

void SecurityOrchestrator::runtimeLoop() {
  for (;;) {
    if (tickSem_) {
      xSemaphoreTake(tickSem_, portMAX_DELAY);
    } else {
      vTaskDelay(pdMS_TO_TICKS(ORCH_TICK_PERIOD_MS));
    }

    if (stateMu_) xSemaphoreTake(stateMu_, portMAX_DELAY);
    tickUnlocked(millis());
    if (stateMu_) xSemaphoreGive(stateMu_);
  }
}

void SecurityOrchestrator::statusLoop() {
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(STATUS_HEARTBEAT_MS);

  for (;;) {
    vTaskDelayUntil(&last, period);
    if (stateMu_) xSemaphoreTake(stateMu_, portMAX_DELAY);
    publishStateStatus("periodic");
    if (stateMu_) xSemaphoreGive(stateMu_);
  }
}

void SecurityOrchestrator::runtimeTaskEntry(void* arg) {
  if (!arg) {
    vTaskDelete(nullptr);
    return;
  }
  static_cast<SecurityOrchestrator*>(arg)->runtimeLoop();
  vTaskDelete(nullptr);
}

void SecurityOrchestrator::tickerTaskEntry(void* arg) {
  if (!arg) {
    vTaskDelete(nullptr);
    return;
  }

  SecurityOrchestrator* self = static_cast<SecurityOrchestrator*>(arg);
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(ORCH_TICK_PERIOD_MS);

  for (;;) {
    if (self->tickSem_) xSemaphoreGive(self->tickSem_);
    vTaskDelayUntil(&last, period);
  }
}

void SecurityOrchestrator::statusTaskEntry(void* arg) {
  if (!arg) {
    vTaskDelete(nullptr);
    return;
  }
  static_cast<SecurityOrchestrator*>(arg)->statusLoop();
  vTaskDelete(nullptr);
}

// ===== FILE: src/main_board\drivers\BuzzerDriver.cpp =====

BuzzerDriver::BuzzerDriver(uint8_t pin, uint8_t channel, uint8_t resolution_bits)
: pin_(pin), ch_(channel), res_(resolution_bits), cur_hz_(0) {}

void BuzzerDriver::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) return;
  // Initialize LEDC once. We'll reconfigure the channel frequency in startTone().
  ledcSetup(ch_, 2000, res_);
  ledcAttachPin(pin_, ch_);
  ledcWrite(ch_, 0);
  cur_hz_ = 0;
}

void BuzzerDriver::startTone(uint32_t hz) {
  if (pin_ == HwCfg::PIN_UNUSED) return;
  if (hz == 0) {
    stopTone();
    return;
  }
  if (cur_hz_ != hz) {
    // Avoid ledcWriteTone(): it can be unreliable across Arduino-ESP32 versions.
    // Reconfigure the channel frequency directly instead.
    ledcSetup(ch_, hz, res_);
    cur_hz_ = hz;
  }
  uint32_t maxDuty = (1u << res_) - 1u;
  // Passive buzzers are typically loudest with ~50% duty (clean square wave).
  ledcWrite(ch_, maxDuty / 2u);
}

void BuzzerDriver::stopTone() {
  if (pin_ == HwCfg::PIN_UNUSED) return;
  ledcWrite(ch_, 0);
  cur_hz_ = 0;
}

// ===== FILE: src/main_board\drivers\I2CKeypadDriver.cpp =====

namespace {
constexpr uint8_t ROW_MASK = 0x0F; // P0..P3
constexpr uint8_t COL_MASK = 0xF0; // P4..P7
}

I2CKeypadDriver::I2CKeypadDriver(TwoWire* wire, uint8_t addr7,
                                 const char* keymap, uint32_t debounce_ms)
: wire_(wire),
  addr7_(addr7),
  keymap_(keymap),
  debounce_ms_(debounce_ms),
  scanRow_(0),
  waitingRelease_(false),
  lastKey_(0),
  lastKeyMs_(0),
  shadow_(0xFF) {}

bool I2CKeypadDriver::writePort_(uint8_t value) {
  if (!wire_) return false;
  wire_->beginTransmission(addr7_);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

int I2CKeypadDriver::readColPressed_() {
  if (!wire_) return -1;
  int n = wire_->requestFrom((int)addr7_, 1);
  if (n != 1) return -1;
  uint8_t v = (uint8_t)wire_->read();

  for (uint8_t c = 0; c < 4; c++) {
    uint8_t bit = (uint8_t)(1u << (4u + c));
    if ((v & bit) == 0) return (int)c;
  }
  return -1;
}

bool I2CKeypadDriver::setAllRowsHigh_() {
  shadow_ = (uint8_t)((shadow_ & (uint8_t)~ROW_MASK) | ROW_MASK);
  return writePort_(shadow_);
}

bool I2CKeypadDriver::setRowActive_(uint8_t r) {
  if (r >= 4) return false;
  shadow_ = (uint8_t)((shadow_ & (uint8_t)~ROW_MASK) | ROW_MASK);
  shadow_ = (uint8_t)(shadow_ & (uint8_t)~(1u << r));
  return writePort_(shadow_);
}

char I2CKeypadDriver::mapKey_(uint8_t r, uint8_t c) const {
  return keymap_[r * 4u + c];
}

bool I2CKeypadDriver::begin() {
  scanRow_ = 0;
  waitingRelease_ = false;
  lastKey_ = 0;
  lastKeyMs_ = 0;
  shadow_ = 0xFF;
  return setAllRowsHigh_();
}

char I2CKeypadDriver::update(uint32_t nowMs) {
  if (waitingRelease_) {
    bool anyDown = false;
    for (uint8_t r = 0; r < 4; r++) {
      if (!setRowActive_(r)) return 0;
      if (readColPressed_() >= 0) {
        anyDown = true;
        break;
      }
    }
    setAllRowsHigh_();
    if (!anyDown) waitingRelease_ = false;
    return 0;
  }

  if (!setRowActive_(scanRow_)) return 0;
  int col = readColPressed_();
  setAllRowsHigh_();

  if (col >= 0) {
    char k = mapKey_(scanRow_, (uint8_t)col);
    if ((nowMs - lastKeyMs_) >= debounce_ms_ || k != lastKey_) {
      lastKey_ = k;
      lastKeyMs_ = nowMs;
      waitingRelease_ = true;
      return k;
    }
  }

  scanRow_ = (uint8_t)((scanRow_ + 1u) & 0x03u);
  return 0;
}

// ===== FILE: src/main_board\drivers\ServoDriver.cpp =====

ServoDriver::ServoDriver(uint8_t pin, uint8_t channel, uint8_t resolution_bits)
: pin_(pin), ch_(channel), res_(resolution_bits), min_us_(500), max_us_(2500) {}

void ServoDriver::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) return;
  ledcSetup(ch_, 50, res_);
  ledcAttachPin(pin_, ch_);
  writePulseUs(1500);
}

uint16_t ServoDriver::clampUs_(uint16_t us) const {
  if (us < min_us_) return min_us_;
  if (us > max_us_) return max_us_;
  return us;
}

void ServoDriver::writePulseUs(uint16_t us) {
  if (pin_ == HwCfg::PIN_UNUSED) return;
  us = clampUs_(us);
  uint32_t maxDuty = (1u << res_) - 1u;
  uint32_t duty = (uint32_t)us * maxDuty / 20000u;
  ledcWrite(ch_, duty);
}

void ServoDriver::writeAngle(uint8_t deg) {
  if (deg > 180) deg = 180;
  uint32_t us = (uint32_t)min_us_ + ((uint32_t)(max_us_ - min_us_) * deg) / 180u;
  writePulseUs((uint16_t)us);
}

// ===== FILE: src/main_board\drivers\UltrasonicDriver.cpp =====

UltrasonicDriver::UltrasonicDriver(uint8_t trigPin, uint8_t echoPin)
: trig_(trigPin), echo_(echoPin) {}

void UltrasonicDriver::begin() {
  if (trig_ == HwCfg::PIN_UNUSED || echo_ == HwCfg::PIN_UNUSED) return;
  pinMode(trig_, OUTPUT);
  pinMode(echo_, INPUT);
  digitalWrite(trig_, LOW);
}

int UltrasonicDriver::readCm(uint32_t timeout_us) {
  if (trig_ == HwCfg::PIN_UNUSED || echo_ == HwCfg::PIN_UNUSED) return -1;
  // trigger pulse
  digitalWrite(trig_, LOW);
  delayMicroseconds(2);
  digitalWrite(trig_, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_, LOW);

  // measure echo pulse
  unsigned long dur = pulseIn(echo_, HIGH, timeout_us);
  if (dur == 0) return -1;

  // speed of sound ~343 m/s => 29.1 us/cm round trip => cm = dur / 58
  int cm = (int)(dur / 58UL);
  return cm;
}

// ===== FILE: src/main_board\pipelines\EventCollector.cpp =====

#ifndef DOOR_CODE
#define DOOR_CODE ""
#endif

namespace {
bool isValidDoorCode(const char* code) {
  if (!code || strlen(code) != 4) return false;
  for (size_t i = 0; i < 4; ++i) {
    if (code[i] < '0' || code[i] > '9') return false;
  }
  return true;
}

bool pinConfigured(uint8_t pin) {
  return pin != HwCfg::PIN_UNUSED;
}
} // namespace

EventCollector::EventCollector()
: us1_(HwCfg::PIN_US_TRIG, HwCfg::PIN_US_ECHO),
  chokep1_(&us1_, 1, 5, 10, 200, 1500),
  us2_(HwCfg::PIN_US_TRIG_2, HwCfg::PIN_US_ECHO_2),
  chokep2_(&us2_, 2, 5, 10, 200, 1500),
  us3_(HwCfg::PIN_US_TRIG_3, HwCfg::PIN_US_ECHO_3),
  chokep3_(&us3_, 3, 5, 10, 200, 1500),
  reedDoor_(HwCfg::PIN_REED_1, 1, EventType::door_open, true, 80),
  reedWindow_(HwCfg::PIN_REED_2, 2, EventType::window_open, true, 80),
  pir1_(HwCfg::PIN_PIR_1, 1, 1500),
  pir2_(HwCfg::PIN_PIR_2, 2, 1500),
  pir3_(HwCfg::PIN_PIR_3, 3, 1500),
  vibCombined_(HwCfg::PIN_VIB_1, 0, 700),
  keypadDrv_(&Wire, HwCfg::KEYPAD_I2C_ADDR, HwCfg::KP_MAP, 60),
  keypadIn_(0) {}

void EventCollector::begin() {
  Wire.begin(HwCfg::PIN_I2C_SDA, HwCfg::PIN_I2C_SCL);
  oled_.begin();
  keypadDrv_.begin();
  keypadIn_.begin();

  serialLineLen_ = 0;
  serialLineLastByteMs_ = 0;

  keypadIn_.setDoorCode(isValidDoorCode(DOOR_CODE) ? DOOR_CODE : "1234");

  const uint32_t nowMs = millis();

  if (pinConfigured(HwCfg::PIN_BTN_DOOR_TOGGLE)) {
    pinMode(HwCfg::PIN_BTN_DOOR_TOGGLE, INPUT_PULLUP);
    const bool pressed = (digitalRead(HwCfg::PIN_BTN_DOOR_TOGGLE) == LOW);
    doorToggleLastRawPressed_ = pressed;
    doorToggleStablePressed_ = pressed;
  }
  doorToggleLastChangeMs_ = nowMs;

  if (pinConfigured(HwCfg::PIN_BTN_WINDOW_TOGGLE)) {
    pinMode(HwCfg::PIN_BTN_WINDOW_TOGGLE, INPUT_PULLUP);
    const bool pressed = (digitalRead(HwCfg::PIN_BTN_WINDOW_TOGGLE) == LOW);
    windowToggleLastRawPressed_ = pressed;
    windowToggleStablePressed_ = pressed;
  }
  windowToggleLastChangeMs_ = nowMs;

  reedDoor_.begin();
  reedWindow_.begin();
  pir1_.begin();
  pir2_.begin();
  pir3_.begin();
  vibCombined_.begin();
  us1_.begin();
  chokep1_.begin();
  us2_.begin();
  chokep2_.begin();
  us3_.begin();
  chokep3_.begin();
}

bool EventCollector::pollKeypad(uint32_t nowMs, Event& out) {
  const char k = keypadDrv_.update(nowMs);
  if (k == 'A') {
    out = {EventType::door_hold_warn_silence, nowMs, 0};
    return true;
  }
  if (k == 'B') {
    out = {EventType::keypad_help_request, nowMs, 0};
    return true;
  }

  if (k) {
    keypadIn_.feedKey(k, nowMs);
    oled_.showCode(keypadIn_.buf(), keypadIn_.len());

    KeypadInput::SubmitResult sr;
    if (keypadIn_.takeSubmitResult(sr)) {
      oled_.showResult(sr == KeypadInput::SubmitResult::ok);
    }
  }

  oled_.update(nowMs);
  return keypadIn_.poll(nowMs, out);
}

void EventCollector::updateOledStatus(uint32_t nowMs,
                                      bool doorLocked,
                                      bool doorOpen,
                                      bool countdownActive,
                                      uint32_t countdownDeadlineMs,
                                      uint32_t countdownWarnBeforeMs) {
  oled_.setDoorStatus(doorLocked,
                      doorOpen,
                      countdownActive,
                      countdownDeadlineMs,
                      countdownWarnBeforeMs);
  oled_.update(nowMs);
}

void EventCollector::printSerialHelp() const {
}

bool EventCollector::parseSerialEvent(char c, uint32_t nowMs, Event& out) const {
  (void)c;
  (void)nowMs;
  (void)out;
  return false;
}

bool EventCollector::parseSerialCode(uint16_t code, uint32_t nowMs, Event& out) const {
  (void)code;
  (void)nowMs;
  (void)out;
  return false;
}

bool EventCollector::parseSerialEvent(const String& token, uint32_t nowMs, Event& out) const {
  (void)token;
  (void)nowMs;
  (void)out;
  return false;
}

bool EventCollector::readSerialEvent(uint32_t nowMs, Event& out) {
  (void)nowMs;
  (void)out;
  return false;
}

bool EventCollector::pollSensorOrSerial(uint32_t nowMs, Event& out) {
  if (pollManualButtons(nowMs, out)) return true;
  if (reedDoor_.poll(nowMs, out)) return true;
  if (reedWindow_.poll(nowMs, out)) return true;
  if (pir1_.poll(nowMs, out)) return true;
  if (pir2_.poll(nowMs, out)) return true;
  if (pir3_.poll(nowMs, out)) return true;
  if (vibCombined_.poll(nowMs, out)) return true;
  if (chokep1_.poll(nowMs, out)) return true;
  if (chokep2_.poll(nowMs, out)) return true;
  if (chokep3_.poll(nowMs, out)) return true;
  return false;
}

bool EventCollector::pollManualButton(uint8_t pin,
                                      uint32_t nowMs,
                                      uint32_t debounceMs,
                                      bool& lastRawPressed,
                                      bool& stablePressed,
                                      uint32_t& lastChangeMs,
                                      EventType pressEvent,
                                      Event& out) {
  if (!pinConfigured(pin)) return false;

  const bool rawPressed = (digitalRead(pin) == LOW);
  if (rawPressed != lastRawPressed) {
    lastRawPressed = rawPressed;
    lastChangeMs = nowMs;
  }

  if ((nowMs - lastChangeMs) < debounceMs) return false;
  if (rawPressed == stablePressed) return false;

  stablePressed = rawPressed;
  if (!stablePressed) return false;

  out = {pressEvent, nowMs, 0};
  return true;
}

bool EventCollector::pollManualButtons(uint32_t nowMs, Event& out) {
  static constexpr uint32_t kDebounceMs = 40;
  if (pollManualButton(HwCfg::PIN_BTN_DOOR_TOGGLE,
                       nowMs,
                       kDebounceMs,
                       doorToggleLastRawPressed_,
                       doorToggleStablePressed_,
                       doorToggleLastChangeMs_,
                       EventType::manual_door_toggle,
                       out)) {
    return true;
  }

  return pollManualButton(HwCfg::PIN_BTN_WINDOW_TOGGLE,
                          nowMs,
                          kDebounceMs,
                          windowToggleLastRawPressed_,
                          windowToggleStablePressed_,
                          windowToggleLastChangeMs_,
                          EventType::manual_window_toggle,
                          out);
}

bool EventCollector::isDoorOpen() const {
  return reedDoor_.isOpen();
}

bool EventCollector::isWindowOpen() const {
  return reedWindow_.isOpen();
}

// ===== FILE: src/main_board\rtos\Queues.cpp =====

namespace RtosQueues {

QueueHandle_t mqttPubQ = nullptr;
QueueHandle_t mqttCmdQ = nullptr;

bool init() {
  if (!mqttPubQ) mqttPubQ = xQueueCreate(16, sizeof(PublishMsg));
  if (!mqttCmdQ) mqttCmdQ = xQueueCreate(8, sizeof(CmdMsg));
  return mqttPubQ && mqttCmdQ;
}

} // namespace RtosQueues

// ===== FILE: src/main_board\rtos\Tasks.cpp =====


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace RtosTasks {

static MqttClient* gMqtt = nullptr;
static TaskHandle_t hMqtt = nullptr;
static bool mqttStarted = false;

static volatile uint32_t gPubDrops = 0;
static volatile uint32_t gCmdDrops = 0;

static void onMqttCommand(const String&, const String& payloadRaw) {
  if (!RtosQueues::mqttCmdQ) return;

  RtosQueues::CmdMsg msg{};
  payloadRaw.toCharArray(msg.payload, sizeof(msg.payload));
  if (xQueueSend(RtosQueues::mqttCmdQ, &msg, 0) != pdTRUE) {
    ++gCmdDrops;
  }
}

static void mqttTask(void*) {
  if (!gMqtt) vTaskDelete(nullptr);

  gMqtt->begin(onMqttCommand);

  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(10);
  uint32_t nextMetricsMs = 0;

  for (;;) {
    const uint32_t nowMs = millis();
    gMqtt->update(nowMs);

    if (RtosQueues::mqttPubQ) {
      RtosQueues::PublishMsg msg{};
      uint32_t burst = 0;
      while (burst < MQTT_PUB_DRAIN_BURST && xQueueReceive(RtosQueues::mqttPubQ, &msg, 0) == pdTRUE) {
        bool ok = false;
        switch (msg.kind) {
          case RtosQueues::PublishKind::event:
            ok = gMqtt->publishEvent(msg.e, msg.st, msg.cmd);
            break;
          case RtosQueues::PublishKind::status:
            ok = gMqtt->publishStatus(msg.st, msg.text1);
            break;
          case RtosQueues::PublishKind::ack:
            ok = gMqtt->publishAck(msg.text1, msg.ok, msg.text2);
            break;
          default:
            ok = false;
            break;
        }
        if (!ok) ++gPubDrops;
        ++burst;
      }
    }

    if ((int32_t)(nowMs - nextMetricsMs) >= 0) {
      nextMetricsMs = nowMs + MQTT_METRICS_PERIOD_MS;
      gMqtt->publishMetrics(
        0,
        gPubDrops,
        gCmdDrops,
        0,
        0,
        RtosQueues::mqttPubQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttPubQ) : 0,
        RtosQueues::mqttCmdQ ? (uint32_t)uxQueueMessagesWaiting(RtosQueues::mqttCmdQ) : 0,
        0
      );
    }

    vTaskDelayUntil(&last, period);
  }
}

void attachMqtt(MqttClient* client) {
  gMqtt = client;
}

void startIfReady() {
  if (!RtosQueues::init()) return;
  if (!gMqtt || mqttStarted) return;

  if (xTaskCreatePinnedToCore(mqttTask, "Mqtt", 4096, nullptr, 1, &hMqtt, 0) == pdPASS) {
    mqttStarted = true;
  }
}

bool mqttWorkerStarted() {
  return mqttStarted;
}

bool enqueuePublish(const RtosQueues::PublishMsg& msg) {
  if (!RtosQueues::mqttPubQ) return false;
  if (xQueueSend(RtosQueues::mqttPubQ, &msg, 0) != pdTRUE) {
    ++gPubDrops;
    return false;
  }
  return true;
}

bool dequeueCommand(RtosQueues::CmdMsg& out) {
  if (!RtosQueues::mqttCmdQ) return false;
  return xQueueReceive(RtosQueues::mqttCmdQ, &out, 0) == pdTRUE;
}

} // namespace RtosTasks

// ===== FILE: src/main_board\sensors\ChokepointSensor.cpp =====

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}
} // namespace

ChokepointSensor::ChokepointSensor(UltrasonicDriver* drv, uint8_t id,
                                   int near_cm, int far_cm,
                                   uint32_t sample_period_ms, uint32_t cooldown_ms)
: drv_(drv),
  id_(id),
  near_cm_(near_cm),
  far_cm_(far_cm),
  sample_period_ms_(sample_period_ms),
  cooldown_ms_(cooldown_ms),
  next_sample_ms_(0),
  last_fire_ms_(0),
  last_cm_(-1),
  inside_(false),
  consecutive_no_echo_(0),
  last_valid_ms_(0),
  seen_valid_once_(false)
{}

void ChokepointSensor::begin() {
  next_sample_ms_ = 0;
  last_fire_ms_ = 0;
  last_cm_ = -1;
  inside_ = false;
  consecutive_no_echo_ = 0;
  last_valid_ms_ = 0;
  seen_valid_once_ = false;
}

int ChokepointSensor::lastCm() const {
  return last_cm_;
}

bool ChokepointSensor::poll(uint32_t nowMs, Event& out) {
  if (!drv_) return false;
  if (next_sample_ms_ != 0 && !reached(nowMs, next_sample_ms_)) return false;

  next_sample_ms_ = nowMs + sample_period_ms_;

  int cm = drv_->readCm();
  last_cm_ = cm;

  if (cm < 0) {
    if (consecutive_no_echo_ < 0xFFFFu) consecutive_no_echo_++;
    return false;
  }
  consecutive_no_echo_ = 0;
  last_valid_ms_ = nowMs;
  seen_valid_once_ = true;

  // hysteresis: enter when <= near, exit when >= far
  if (!inside_) {
    if (cm <= near_cm_) {
      inside_ = true;

      if ((nowMs - last_fire_ms_) >= cooldown_ms_) {
        last_fire_ms_ = nowMs;
        out = {EventType::chokepoint, nowMs, id_};
        return true;
      }
    }
  } else {
    if (cm >= far_cm_) {
      inside_ = false;
    }
  }

  return false;
}

bool ChokepointSensor::isOffline(uint32_t nowMs, uint32_t noValidMs, uint16_t noEchoCount) const {
  if (!seen_valid_once_) return false;
  const bool noEchoTooMany = (noEchoCount > 0 && consecutive_no_echo_ >= noEchoCount);
  if (noValidMs == 0) return noEchoTooMany;

  const bool noValidTooLong = (int32_t)(nowMs - (last_valid_ms_ + noValidMs)) >= 0;
  return noEchoTooMany || noValidTooLong;
}

uint16_t ChokepointSensor::consecutiveNoEcho() const {
  return consecutive_no_echo_;
}

uint32_t ChokepointSensor::lastValidMs() const {
  return last_valid_ms_;
}

// ===== FILE: src/main_board\sensors\KeypadInput.cpp =====

KeypadInput::KeypadInput(uint8_t id)
: id_(id),
  len_(0),
  hasEvent_(false),
  pending_{EventType::disarm, 0, 0},
  lastKeyMs_(0),
  lastSubmit_(SubmitResult::none) {
  // Safe default until configured from env: keypad unlock disabled.
  strncpy(doorCode_, "ABCD", 5);
  clear_();
}

void KeypadInput::begin() {
  clear_();
  hasEvent_ = false;
  lastKeyMs_ = 0;
  lastSubmit_ = SubmitResult::none;
}

void KeypadInput::setDoorCode(const char* code4) {
  strncpy(doorCode_, code4, 4);
  doorCode_[4] = '\0';
}

void KeypadInput::clear_() {
  len_ = 0;
  buf_[0] = '\0';
}

bool KeypadInput::match_(const char* a, const char* b) const {
  return strncmp(a, b, 4) == 0;
}

void KeypadInput::feedKey(char k, uint32_t nowMs) {
  // à¸à¸±à¸™ key à¹€à¸”à¹‰à¸‡à¸–à¸µà¹ˆà¹€à¸à¸´à¸™ (à¹€à¸œà¸·à¹ˆà¸­à¸ªà¸²à¸¢/à¸„à¸­à¸™à¹à¸—à¸„)
  if ((nowMs - lastKeyMs_) < 30) return;
  lastKeyMs_ = nowMs;

  if (k == '*' || k == 'D') {
    // backspace
    if (len_ > 0) {
      len_--;
      buf_[len_] = '\0';
    }
    return;
  }

  if (k == 'C') { clear_(); return; }

  if (k >= '0' && k <= '9') {
    if (len_ < 4) {
      buf_[len_++] = k;
      buf_[len_] = '\0';
    }
    return;
  }

  if (k == '#') {
    if (len_ == 4 && match_(buf_, doorCode_)) {
      pending_ = {EventType::door_code_unlock, nowMs, id_};
      hasEvent_ = true;
      lastSubmit_ = SubmitResult::ok;
    } else {
      lastSubmit_ = SubmitResult::bad;
      pending_ = {EventType::door_code_bad, nowMs, id_};
      hasEvent_ = true;
    }
    clear_();
    return;
  }
}

bool KeypadInput::takeSubmitResult(SubmitResult& out) {
  if (lastSubmit_ == SubmitResult::none) return false;
  out = lastSubmit_;
  lastSubmit_ = SubmitResult::none;
  return true;
}

bool KeypadInput::poll(uint32_t, Event& out) {
  if (!hasEvent_) return false;
  hasEvent_ = false;
  out = pending_;
  return true;
}

// ===== FILE: src/main_board\sensors\PirSensor.cpp =====

PirSensor::PirSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0),
  last_active_(false),
  seen_inactive_since_begin_(false)
{}

void PirSensor::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) {
    last_fire_ms_ = 0;
    last_active_ = false;
    active_since_ms_ = 0;
    seen_inactive_since_begin_ = true;
    return;
  }
  pinMode(pin_, INPUT);
  last_fire_ms_ = 0;
  last_active_ = (digitalRead(pin_) == HIGH);
  active_since_ms_ = last_active_ ? millis() : 0;
  // Require at least one observed inactive sample before flagging stuck-active.
  // This prevents false sensor-faults when a channel boots floating/high.
  seen_inactive_since_begin_ = !last_active_;
}

bool PirSensor::poll(uint32_t nowMs, Event& out) {
  if (pin_ == HwCfg::PIN_UNUSED) return false;
  const bool active = (digitalRead(pin_) == HIGH);
  if (active && !last_active_) {
    active_since_ms_ = nowMs;
  } else if (!active) {
    active_since_ms_ = 0;
    seen_inactive_since_begin_ = true;
  }
  const bool rising_edge = (active && !last_active_);
  last_active_ = active;

  if (!rising_edge) return false;
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  last_fire_ms_ = nowMs;
  out = {EventType::motion, nowMs, id_};
  return true;
}

bool PirSensor::isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const {
  if (!seen_inactive_since_begin_ || !last_active_ || active_since_ms_ == 0 || thresholdMs == 0) return false;
  return (int32_t)(nowMs - (active_since_ms_ + thresholdMs)) >= 0;
}

// ===== FILE: src/main_board\sensors\ReedSensor.cpp =====

ReedSensor::ReedSensor(uint8_t pin, uint8_t id, EventType open_event, bool open_is_high, uint32_t debounce_ms)
: pin_(pin),
  id_(id),
  open_event_(open_event),
  open_is_high_(open_is_high),
  debounce_ms_(debounce_ms),
  stable_open_(false),
  last_raw_(false),
  last_flip_ms_(0),
  fired_open_(false)
{}

void ReedSensor::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) {
    stable_open_ = false;
    last_raw_ = false;
    last_flip_ms_ = millis();
    fired_open_ = false;
    return;
  }
  pinMode(pin_, INPUT_PULLUP);
  stable_open_ = readOpenRaw_();
  last_raw_ = stable_open_;
  last_flip_ms_ = millis();
  fired_open_ = false;
}

bool ReedSensor::poll(uint32_t nowMs, Event& out) {
  if (pin_ == HwCfg::PIN_UNUSED) return false;
  bool raw = readOpenRaw_();

  if (raw != last_raw_) {
    last_raw_ = raw;
    last_flip_ms_ = nowMs;
  }

  if ((nowMs - last_flip_ms_) < debounce_ms_) return false;

  if (stable_open_ != raw) {
    stable_open_ = raw;
    if (stable_open_) fired_open_ = false;
  }

  if (stable_open_ && !fired_open_) {
    fired_open_ = true;
    out = {open_event_, nowMs, id_};
    return true;
  }

  return false;
}

bool ReedSensor::isOpen() const {
  return stable_open_;
}

bool ReedSensor::readOpenRaw_() const {
  if (pin_ == HwCfg::PIN_UNUSED) return false;
  int v = digitalRead(pin_);
  bool high = (v == HIGH);
  return open_is_high_ ? high : !high;
}

// ===== FILE: src/main_board\sensors\VibrationSensor.cpp =====

VibrationSensor::VibrationSensor(uint8_t pin, uint8_t id, uint32_t cooldown_ms)
: pin_(pin),
  id_(id),
  cooldown_ms_(cooldown_ms),
  last_fire_ms_(0),
  last_active_(false),
  seen_inactive_since_begin_(false)
{}

void VibrationSensor::begin() {
  if (pin_ == HwCfg::PIN_UNUSED) {
    last_fire_ms_ = 0;
    last_active_ = false;
    active_since_ms_ = 0;
    seen_inactive_since_begin_ = true;
    return;
  }
  pinMode(pin_, INPUT_PULLUP);
  last_fire_ms_ = 0;
  // INPUT_PULLUP means an open circuit reads HIGH. We only fire on a transition.
  last_active_ = (digitalRead(pin_) == HIGH);
  active_since_ms_ = last_active_ ? millis() : 0;
  // Require at least one observed inactive sample before flagging stuck-active.
  // This prevents false sensor-faults when a channel boots floating/high.
  seen_inactive_since_begin_ = !last_active_;
}

bool VibrationSensor::poll(uint32_t nowMs, Event& out) {
  if (pin_ == HwCfg::PIN_UNUSED) return false;
  const bool active = (digitalRead(pin_) == HIGH);
  if (active && !last_active_) {
    active_since_ms_ = nowMs;
  } else if (!active) {
    active_since_ms_ = 0;
    seen_inactive_since_begin_ = true;
  }
  const bool rising_edge = (active && !last_active_);
  last_active_ = active;

  if (!rising_edge) return false;
  if ((nowMs - last_fire_ms_) < cooldown_ms_) return false;

  last_fire_ms_ = nowMs;
  out = {EventType::vib_spike, nowMs, id_};
  return true;
}

bool VibrationSensor::isStuckActive(uint32_t nowMs, uint32_t thresholdMs) const {
  if (!seen_inactive_since_begin_ || !last_active_ || active_since_ms_ == 0 || thresholdMs == 0) return false;
  return (int32_t)(nowMs - (active_since_ms_ + thresholdMs)) >= 0;
}

// ===== FILE: src/main_board\services\CommandDispatcher.cpp =====

static bool isDisarmed(const SystemState& st) {
  return st.mode == Mode::disarm;
}

void applyCommand(const Command& cmd, const SystemState& st, Actuators& acts, Logger* logger) {
  if (st.mode == Mode::startup_safe) {
    if (acts.buzzer) acts.buzzer->stop();
    if (acts.servo1) acts.servo1->lock();
    if (acts.servo2) acts.servo2->lock();
  } else if (isDisarmed(st)) {
    if (acts.buzzer) acts.buzzer->stop();
    if (acts.servo2) {
      // In disarm mode, keep locks as-is unless policy explicitly requires window lock.
      if (st.keep_window_locked_when_disarmed) acts.servo2->lock();
    }
  } else {
    if (acts.servo1) acts.servo1->lock();
    if (acts.servo2) acts.servo2->lock();
  }

  switch (cmd.type) {
    case CommandType::buzzer_warn:
      if (acts.buzzer) acts.buzzer->warn();
      break;

    case CommandType::buzzer_alert:
      if (acts.buzzer) acts.buzzer->alert();
      break;

    case CommandType::servo_lock:
      if (acts.servo1) acts.servo1->lock();
      if (acts.servo2) acts.servo2->lock();
      break;

    case CommandType::none:
    default:
      break;
  }

  if (logger) logger->logCommand(cmd, st);
}

// ===== FILE: src/main_board\services\Logger.cpp =====

void Logger::begin() {}

void Logger::logCommand(const Command& cmd, const SystemState& st) {
  (void)cmd;
  (void)st;
}

// ===== FILE: src/main_board\services\MqttBus.cpp =====

#include <cstring>


namespace {
MqttClient gClient;
}

void MqttBus::begin() {
  RtosTasks::attachMqtt(&gClient);
  RtosTasks::startIfReady();
  rtosActive_ = RtosTasks::mqttWorkerStarted();
}

void MqttBus::publishEvent(const Event& e, const SystemState& st, const Command& cmd) {
  if (!rtosActive_) return;
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::event;
  msg.e = e;
  msg.st = st;
  msg.cmd = cmd;
  RtosTasks::enqueuePublish(msg);
}

void MqttBus::publishStatus(const SystemState& st, const char* reason) {
  if (!rtosActive_) return;
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::status;
  msg.st = st;
  if (reason) {
    std::strncpy(msg.text1, reason, sizeof(msg.text1) - 1);
    msg.text1[sizeof(msg.text1) - 1] = '\0';
  }
  RtosTasks::enqueuePublish(msg);
}

void MqttBus::publishAck(const char* cmd, bool ok, const char* detail) {
  if (!rtosActive_) return;
  RtosQueues::PublishMsg msg{};
  msg.kind = RtosQueues::PublishKind::ack;
  msg.ok = ok;
  if (cmd) {
    std::strncpy(msg.text1, cmd, sizeof(msg.text1) - 1);
    msg.text1[sizeof(msg.text1) - 1] = '\0';
  }
  if (detail) {
    std::strncpy(msg.text2, detail, sizeof(msg.text2) - 1);
    msg.text2[sizeof(msg.text2) - 1] = '\0';
  }
  RtosTasks::enqueuePublish(msg);
}

bool MqttBus::pollCommand(String& outPayload) {
  outPayload = "";
  if (!rtosActive_) return false;
  RtosQueues::CmdMsg msg{};
  if (!RtosTasks::dequeueCommand(msg)) return false;
  outPayload = String(msg.payload);
  return true;
}

// ===== FILE: src/main_board\services\MqttClient.cpp =====

MqttClient* MqttClient::self_ = nullptr;

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

const char* levelText(AlarmLevel lv) {
  switch (lv) {
    case AlarmLevel::off:   return "off";
    case AlarmLevel::warn:  return "warn";
    case AlarmLevel::alert: return "alert";
    default:                return "off";
  }
}
} // namespace

MqttClient::MqttClient()
: mqtt_(wifiClient_) {}

void MqttClient::begin(CommandCallback cb) {
  cmdCb_ = cb;
  self_ = this;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  mqtt_.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt_.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt_.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt_.setCallback(onMqttMessage);
}

void MqttClient::connectWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!reached(nowMs, nextWifiRetryMs_)) return;

  nextWifiRetryMs_ = nowMs + WIFI_RECONNECT_MS;
  if (strlen(WIFI_SSID) == 0) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void MqttClient::connectMqtt(uint32_t nowMs) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt_.connected()) return;
  if (!reached(nowMs, nextMqttRetryMs_)) return;

  nextMqttRetryMs_ = nowMs + MQTT_RECONNECT_MS;

  const bool hasAuth = strlen(MQTT_USERNAME) > 0;
  bool ok = false;

  if (hasAuth) {
    ok = mqtt_.connect(
      MQTT_CLIENT_ID,
      MQTT_USERNAME,
      MQTT_PASSWORD,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"reason\":\"offline\"}"
    );
  } else {
    ok = mqtt_.connect(
      MQTT_CLIENT_ID,
      MQTT_TOPIC_STATUS,
      1,
      true,
      "{\"reason\":\"offline\"}"
    );
  }

  if (!ok) return;

  mqtt_.subscribe(MQTT_TOPIC_CMD);
  mqtt_.publish(MQTT_TOPIC_STATUS, "{\"reason\":\"online\"}", false);
}

void MqttClient::update(uint32_t nowMs) {
  connectWifi(nowMs);
  connectMqtt(nowMs);
  if (mqtt_.connected()) mqtt_.loop();
}

bool MqttClient::ready() {
  return mqtt_.connected();
}

bool MqttClient::publishEvent(const Event& e, const SystemState& st, const Command& cmd) {
  if (!ready()) return false;

  String payload = "{\"event\":\"";
  payload += toString(e.type);
  payload += "\",\"src\":";
  payload += String(e.src);
  payload += ",\"cmd\":\"";
  payload += toString(cmd.type);
  payload += "\",\"mode\":\"";
  payload += toString(st.mode);
  payload += "\",\"level\":\"";
  payload += levelText(st.level);
  payload += "\",\"door_locked\":";
  payload += st.door_locked ? "true" : "false";
  payload += ",\"window_locked\":";
  payload += st.window_locked ? "true" : "false";
  payload += ",\"door_open\":";
  payload += st.door_open ? "true" : "false";
  payload += ",\"window_open\":";
  payload += st.window_open ? "true" : "false";
  payload += ",\"ts_ms\":";
  payload += String(e.ts_ms);
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_EVENT, payload.c_str(), true);
}

bool MqttClient::publishStatus(const SystemState& st, const char* reason) {
  if (!ready()) return false;

  String payload = "{\"reason\":\"";
  payload += (reason ? reason : "state");
  payload += "\",\"mode\":\"";
  payload += toString(st.mode);
  payload += "\",\"level\":\"";
  payload += levelText(st.level);
  payload += "\",\"door_locked\":";
  payload += st.door_locked ? "true" : "false";
  payload += ",\"window_locked\":";
  payload += st.window_locked ? "true" : "false";
  payload += ",\"door_open\":";
  payload += st.door_open ? "true" : "false";
  payload += ",\"window_open\":";
  payload += st.window_open ? "true" : "false";
  payload += ",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_STATUS, payload.c_str(), true);
}

bool MqttClient::publishAck(const char* cmd, bool ok, const char* detail) {
  if (!ready()) return false;

  String payload = "{\"cmd\":\"";
  payload += (cmd ? cmd : "");
  payload += "\",\"ok\":";
  payload += ok ? "true" : "false";
  payload += ",\"detail\":\"";
  payload += (detail ? detail : "");
  payload += "\",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_ACK, payload.c_str(), false);
}

bool MqttClient::publishMetrics(
  uint32_t usDrops,
  uint32_t pubDrops,
  uint32_t cmdDrops,
  uint32_t storeDrops,
  uint32_t usQueueDepth,
  uint32_t pubQueueDepth,
  uint32_t cmdQueueDepth,
  uint32_t storeDepth
) {
  if (!ready()) return false;

  String payload = "{\"us_drops\":";
  payload += String(usDrops);
  payload += ",\"pub_drops\":";
  payload += String(pubDrops);
  payload += ",\"cmd_drops\":";
  payload += String(cmdDrops);
  payload += ",\"store_drops\":";
  payload += String(storeDrops);
  payload += ",\"q_us\":";
  payload += String(usQueueDepth);
  payload += ",\"q_pub\":";
  payload += String(pubQueueDepth);
  payload += ",\"q_cmd\":";
  payload += String(cmdQueueDepth);
  payload += ",\"q_store\":";
  payload += String(storeDepth);
  payload += ",\"uptime_ms\":";
  payload += String(millis());
  payload += "}";

  return mqtt_.publish(MQTT_TOPIC_METRICS, payload.c_str(), false);
}

void MqttClient::onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (!self_ || !self_->cmdCb_) return;

  String t = topic ? String(topic) : String("");
  String p;
  p.reserve(length);
  for (unsigned int i = 0; i < length; ++i) {
    p += (char)payload[i];
  }

  self_->cmdCb_(t, p);
}

// ===== FILE: src/main_board\ui\OledCodeUi.cpp =====

#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace {
inline bool reached(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) >= 0;
}

inline bool beforeOrAt(uint32_t nowMs, uint32_t targetMs) {
  return (int32_t)(nowMs - targetMs) <= 0;
}

inline uint32_t remainingMs(uint32_t nowMs, uint32_t targetMs) {
  const int32_t delta = (int32_t)(targetMs - nowMs);
  return (delta > 0) ? (uint32_t)delta : 0u;
}
} // namespace

OledCodeUi::OledCodeUi(uint8_t addr7, uint8_t w, uint8_t h)
: addr7_(addr7),
  w_(w),
  h_(h),
  disp_(nullptr),
  len_(0),
  showing_result_(false),
  last_ok_(false),
  result_until_ms_(0) {
  code_[0] = '\0';
}

bool OledCodeUi::begin() {
  if (disp_) return true;

  // -1 reset pin: common I2C modules omit reset.
  disp_ = new Adafruit_SSD1306(w_, h_, &Wire, -1);
  if (!disp_) return false;

  // Use explicit I2C address.
  if (!disp_->begin(SSD1306_SWITCHCAPVCC, addr7_)) {
    delete disp_;
    disp_ = nullptr;
    return false;
  }

  disp_->clearDisplay();
  disp_->setTextColor(SSD1306_WHITE);
  disp_->setTextSize(1);
  disp_->setCursor(0, 0);
  disp_->println("EmbeddedSecurity");
  disp_->println("Keypad ready");
  disp_->display();
  delay(250);

  render_();
  return true;
}

void OledCodeUi::showCode(const char* code, uint8_t len) {
  if (!disp_) return;
  if (!code) code = "";
  if (len > 4) len = 4;

  len_ = len;
  for (uint8_t i = 0; i < len_; ++i) {
    code_[i] = code[i];
  }
  code_[len_] = '\0';

  // If user is typing, hide result screen.
  showing_result_ = false;
  result_until_ms_ = 0;
  dirty_ = true;
  render_();
}

void OledCodeUi::showResult(bool ok) {
  if (!disp_) return;
  showing_result_ = true;
  last_ok_ = ok;
  result_until_ms_ = millis() + 1200;
  dirty_ = true;
  render_();
}

void OledCodeUi::setDoorStatus(bool doorLocked,
                               bool doorOpen,
                               bool countdownActive,
                               uint32_t countdownDeadlineMs,
                               uint32_t countdownWarnBeforeMs) {
  if (!disp_) return;

  bool changed = false;
  if (doorLocked_ != doorLocked) { doorLocked_ = doorLocked; changed = true; }
  if (doorOpen_ != doorOpen) { doorOpen_ = doorOpen; changed = true; }

  if (countdownActive_ != countdownActive) { countdownActive_ = countdownActive; changed = true; }
  if (countdownDeadlineMs_ != countdownDeadlineMs) { countdownDeadlineMs_ = countdownDeadlineMs; changed = true; }
  if (countdownWarnBeforeMs_ != countdownWarnBeforeMs) { countdownWarnBeforeMs_ = countdownWarnBeforeMs; changed = true; }

  if (changed) {
    dirty_ = true;
    render_();
  }
}

void OledCodeUi::update(uint32_t nowMs) {
  if (!disp_) return;
  if (showing_result_ && result_until_ms_ != 0 && reached(nowMs, result_until_ms_)) {
    showing_result_ = false;
    result_until_ms_ = 0;
    dirty_ = true;
    render_();
    return;
  }

  // Re-render when countdown display should change (once per second).
  int secLeft = -1;
  bool urgent = false;
  if (countdownActive_ && countdownDeadlineMs_ != 0 && beforeOrAt(nowMs, countdownDeadlineMs_)) {
    const uint32_t msLeft = remainingMs(nowMs, countdownDeadlineMs_);
    secLeft = (int)((msLeft + 999u) / 1000u);
    urgent = (countdownWarnBeforeMs_ != 0) && ((uint32_t)secLeft * 1000u <= countdownWarnBeforeMs_);
  }

  if (dirty_ || secLeft != lastCountdownSec_ || urgent != lastCountdownUrgent_) {
    lastCountdownSec_ = secLeft;
    lastCountdownUrgent_ = urgent;
    render_();
  }
}

void OledCodeUi::render_() {
  if (!disp_) return;

  dirty_ = false;

  disp_->clearDisplay();
  disp_->setTextColor(SSD1306_WHITE);
  disp_->setTextSize(1);
  disp_->setCursor(0, 0);
  disp_->print("DOOR: ");
  disp_->print(doorLocked_ ? "LOCK" : "UNLOCK");
  if (doorOpen_) disp_->print(" OPEN");

  const uint32_t nowMs = millis();
  if (countdownActive_ && countdownDeadlineMs_ != 0 && beforeOrAt(nowMs, countdownDeadlineMs_)) {
    const uint32_t msLeft = remainingMs(nowMs, countdownDeadlineMs_);
    const uint32_t secLeft = (msLeft + 999u) / 1000u;
    disp_->print(" ");
    disp_->print(secLeft);
    disp_->print("s");
    const bool urgent = (countdownWarnBeforeMs_ != 0) && (secLeft * 1000u <= countdownWarnBeforeMs_);
    if (urgent) disp_->print("!");
  }
  disp_->println();

  disp_->println("PIN:");

  disp_->setTextSize(2);
  disp_->setCursor(0, 16);
  if (len_ == 0) {
    disp_->println("____");
  } else {
    // Show the digits as entered (per request).
    disp_->print(code_);
    for (uint8_t i = len_; i < 4; ++i) disp_->print('_');
    disp_->println();
  }

  disp_->setTextSize(2);
  disp_->setCursor(0, 44);
  if (showing_result_) {
    disp_->print(last_ok_ ? "OK" : "ERR");
  } else {
    disp_->print("    ");
  }

  disp_->display();
}

// ===== FILE: src/main_board\main.cpp =====

static SecurityOrchestrator orchestrator;

void setup() {
  delay(200);
  orchestrator.begin();
}

void loop() {
  orchestrator.tick(millis());
}

// ---- END FLATTENED CONTENT ----
