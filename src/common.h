#include <Arduino.h>

#ifndef PITES_COMMON_FUNCTIONS_H_
#define PITES_COMMON_FUNCTIONS_H_

void ledOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

void ledOn() {
  digitalWrite(LED_BUILTIN, LOW);
}


void blinkSpecificDelay(int times, int delayMs) {
  for(int i=0; i<times; i++) {
    ledOn();
    delay(delayMs);
    ledOff();
    delay(delayMs);
  }
}

void blink(int times) {
  blinkSpecificDelay(times, 500);
}

void blinkFast(int times) {
  blinkSpecificDelay(times, 100);
}

void blinkVeryFast(int times) {
  blinkSpecificDelay(times, 50);
}

#endif
