#include "temp_system.h"
#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 5
#define DHTTYPE DHT11

#define FAN_PIN 18

#define TEMP_ON 30
#define TEMP_OFF 27

DHT dht(DHTPIN, DHTTYPE);

bool fanState = false;

void initTempSystem() {
    dht.begin();
    ledcAttachPin(FAN_PIN, 0);
    ledcSetup(0, 5000, 8);  // channel 0, 5kHz, 8-bit
}

void updateTempSystem() {
    float temp = dht.readTemperature();

    if (isnan(temp)) {
        Serial.println("DHT error");
        return;
    }

    Serial.print("Temp: ");
    Serial.println(temp);

    if (!fanState && temp >= TEMP_ON) {
        ledcWrite(0, 200);  // เปิดพัดลม 80%
        fanState = true;
    }

    if (fanState && temp <= TEMP_OFF) {
        ledcWrite(0, 0);  // ปิดพัดลม
        fanState = false;
    }
}
