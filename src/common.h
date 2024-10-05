#include <Arduino.h>

#ifndef COMMON_FUNCTIONS_H_
#define COMMON_FUNCTIONS_H_

#define LED_BUILTIN 4

void ledOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

void ledOn() {
  digitalWrite(LED_BUILTIN, LOW);
}


void blink(int times) {
  for(int i=0; i<times; i++) {
    ledOn();
    delay(200);
    ledOff();
    delay(200);
  }
}

void blinkFast(int times) {
  for(int i=0; i<times; i++) {
    ledOn();
    delay(100);
    ledOff();
    delay(100);
  }
}

#endif
