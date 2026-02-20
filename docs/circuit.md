# Circuit (Single-Board Main Firmware)

## 1) Wiring Diagram (Logical)

```mermaid
flowchart LR
  ESP[ESP32 DevKit V1]

  BUZ[Buzzer]
  SERVO_D[Door Servo]
  SERVO_W[Window Servo]

  REED_D[Door Reed]
  REED_W[Window Reed]
  PIR1[PIR Zone A Door and Window Room]
  PIR2[PIR Zone B Interior Room]
  PIR3[PIR Outdoor Zone]
  VIB[Vibration Sensor]
  US1[Ultrasonic Door Zone]
  US2[Ultrasonic Window Zone]
  US3[Ultrasonic Between Room Zone]
  BTN_D[Manual Door Button]
  BTN_W[Manual Window Button]
  KP[PCF8574 Keypad 4x4]
  OLED[SSD1306 OLED]

  ESP -->|GPIO25| BUZ
  ESP -->|GPIO26 PWM| SERVO_D
  ESP -->|GPIO27 PWM| SERVO_W

  ESP -->|GPIO32 input| REED_D
  ESP -->|GPIO19 input| REED_W
  ESP -->|GPIO35 input| PIR1
  ESP -->|GPIO36 input| PIR2
  ESP -->|GPIO39 input| PIR3
  ESP -->|GPIO34 input| VIB

  ESP -->|TRIG 13 ECHO 14| US1
  ESP -->|TRIG 16 ECHO 17| US2
  ESP -->|TRIG 4 ECHO 5| US3

  ESP -->|GPIO33 input pull-up| BTN_D
  ESP -->|GPIO18 input pull-up| BTN_W

  ESP <-->|I2C SDA21 SCL22| KP
  ESP <-->|I2C SDA21 SCL22| OLED
```

## 2) Pin Map

| Module | Signal | ESP32 Pin | Notes |
|---|---|---|---|
| Buzzer | PWM | GPIO25 | LEDC tone output |
| Door servo | PWM | GPIO26 | 50Hz servo PWM |
| Window servo | PWM | GPIO27 | 50Hz servo PWM |
| Door reed | Digital in | GPIO32 | Event: `door_open` |
| Window reed | Digital in | GPIO19 | Event: `window_open` |
| PIR Zone A | Digital in | GPIO35 | Room that contains door and window |
| PIR Zone B | Digital in | GPIO36 | Other indoor room |
| PIR Outdoor Zone | Digital in | GPIO39 | Outdoor zone |
| Vibration | Digital in | GPIO34 | Event: `vib_spike` |
| Ultrasonic Door Zone | TRIG / ECHO | GPIO13 / GPIO14 | Chokepoint at main entry door |
| Ultrasonic Window Zone | TRIG / ECHO | GPIO16 / GPIO17 | Chokepoint at window side |
| Ultrasonic Between Room Zone | TRIG / ECHO | GPIO4 / GPIO5 | Chokepoint between rooms |
| Manual door button | Digital in | GPIO33 | Active LOW with pull-up |
| Manual window button | Digital in | GPIO18 | Active LOW with pull-up |
| Keypad (PCF8574) | I2C SDA/SCL | GPIO21 / GPIO22 | I2C addr `0x20` |
| OLED SSD1306 | I2C SDA/SCL | GPIO21 / GPIO22 | I2C addr `0x3C` |

## 3) Electrical Notes

- Use external 5V supply for servos and share GND with ESP32.
- If ultrasonic modules are 5V (for example HC-SR04), protect ESP32 ECHO pins with level shifting or resistor divider to 3.3V-safe input.
- GPIO34/35/36/39 are input-only pins and suitable for sensor inputs.
- Buttons are configured as `INPUT_PULLUP`, so wiring should short to GND when pressed.

## 4) Sensor Position Mapping (for report/demo)

| Sensor Name | Position Definition |
|---|---|
| Ultrasonic Door Zone | Main entry door chokepoint |
| Ultrasonic Window Zone | Window-side chokepoint |
| Ultrasonic Between Room Zone | Between-room chokepoint |
| PIR Zone A | Room that has both door and window |
| PIR Zone B | Other indoor room |
| PIR Outdoor Zone | Outdoor perimeter side |
