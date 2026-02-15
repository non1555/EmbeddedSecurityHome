#include <Arduino.h>

#define LDR_PIN 34
#define RELAY_LIGHT 26

#define LIGHT_THRESHOLD 1500   // ค่า ADC ต้องปรับตามจริง

extern bool isSomeoneHome;     // รับค่าจาก Presence Module

void initLightSystem() {
    pinMode(RELAY_LIGHT, OUTPUT);
}

void updateLightSystem() {

    int lightValue = analogRead(LDR_PIN);

    Serial.print("Light: ");
    Serial.println(lightValue);

    if (isSomeoneHome && lightValue > LIGHT_THRESHOLD) {
        digitalWrite(RELAY_LIGHT, HIGH);   // เปิดไฟ
    } 
    else {
        digitalWrite(RELAY_LIGHT, LOW);    // ปิดไฟ
    }
}
