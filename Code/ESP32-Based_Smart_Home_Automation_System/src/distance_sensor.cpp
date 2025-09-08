#include <Arduino.h>
#include "distance_sensor.h"

#define TRIG_PIN 12  // 触发信号 GPIO
#define ECHO_PIN 18 // 回响信号 GPIO（需要 5V → 3.3V 分压）

static int trigPinGlobal;
static int echoPinGlobal;

void initDistanceSensor() {
  trigPinGlobal = TRIG_PIN;
  echoPinGlobal = ECHO_PIN;
  pinMode(trigPinGlobal, OUTPUT);
  pinMode(echoPinGlobal, INPUT);
}

float measureDistanceCM() {
  digitalWrite(trigPinGlobal, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPinGlobal, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinGlobal, LOW);

  long duration = pulseIn(echoPinGlobal, HIGH);
  float distance = duration * 0.0343 / 2;
  return distance;
}
