#include "UltrasonicDriver.h"

UltrasonicDriver::UltrasonicDriver(uint8_t trigPin, uint8_t echoPin)
: trig_(trigPin), echo_(echoPin) {}

void UltrasonicDriver::begin() {
  pinMode(trig_, OUTPUT);
  pinMode(echo_, INPUT);
  digitalWrite(trig_, LOW);
}

int UltrasonicDriver::readCm(uint32_t timeout_us) {
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
