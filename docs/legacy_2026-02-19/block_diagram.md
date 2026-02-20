# Block Diagram (EmbeddedSecurityHome)

เอกสารนี้เป็น Block Diagram สำหรับใช้ในรายงาน/พรีเซนต์ โดยเน้นโครงระบบและความสัมพันธ์ของแต่ละบอร์ด

## 1) System-Level Block Diagram

```mermaid
flowchart LR
  U[User]
  LINE[LINE OA]
  BR[LINE Bridge<br/>FastAPI + MQTT Client]
  MQ[MQTT Broker]
  MAIN[Main Board ESP32<br/>Security Logic]
  AUTO[Automation Board ESP32<br/>Light/Fan Logic]

  SSEC[Sensors: Reed, PIR, Vibration,<br/>Ultrasonic, Keypad, Buttons]
  ASEC[Actuators: Door/Window Servo,<br/>Buzzer, OLED]
  SAUTO[Sensors: BH1750, DHT]
  AAUTO[Actuators: Light LED, Fan(L293D)]

  U --> LINE
  LINE --> BR
  BR <--> MQ
  MQ <--> MAIN
  MQ <--> AUTO

  SSEC --> MAIN --> ASEC
  SAUTO --> AUTO --> AAUTO
```

## 2) Main Board Internal Block Diagram

```mermaid
flowchart LR
  IN1[Sensors + Keypad + Manual Buttons + Serial]
  EC[EventCollector]
  TS[TimeoutScheduler]
  ORCH[SecurityOrchestrator]
  RE[RuleEngine]
  DD[CommandDispatcher]
  DUS[DoorUnlockSession]
  AC1[Servo/Buzzer/OLED]
  MB[MqttBus/MqttClient]
  NT[Notify Service]

  IN1 --> EC --> ORCH
  TS --> ORCH
  ORCH --> RE --> DD --> AC1
  ORCH --> DUS --> AC1
  ORCH <--> MB
  ORCH --> NT
  NT --> MB
```

## 3) Automation Board Internal Block Diagram

```mermaid
flowchart LR
  IN2[BH1750 Lux + DHT Temp/Humidity]
  CTX[Main Context via MQTT<br/>mode + someone_home]
  AR[AutomationRuntime]
  PR[Presence State]
  AP[AutomationPipeline<br/>light_system + temp_system]
  OA[OutputActuator]
  OUT[Light LED + Fan(L293D)]
  AMQ[Auto MQTT Status/Ack]

  IN2 --> AR
  CTX --> AR
  AR --> PR
  AR --> AP --> OA --> OUT
  AR --> AMQ
```

## 4) Communication Block (Topics)

```mermaid
flowchart TB
  C1[LINE Bridge] -->|publish| T1[esh/main/cmd]
  T2[esh/main/event] -->|subscribe| C1
  T3[esh/main/status] -->|subscribe| C1
  T4[esh/main/ack] -->|subscribe| C1
  T5[esh/auto/status] -->|subscribe| C1
  T6[esh/auto/ack] -->|subscribe| C1

  M1[Main Board] -->|publish| T2
  M1 -->|publish| T3
  M1 -->|publish| T4
  M1 -->|subscribe| T1

  A1[Automation Board] -->|publish| T5
  A1 -->|publish| T6
  A1 -->|subscribe| T3
  A1 -->|subscribe| T7[esh/auto/cmd]
  C1 -->|publish| T7
```
