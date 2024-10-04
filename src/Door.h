#define PIN_MOTOR_UP D1
#define PIN_MOTOR_DOWN D2
#define PIN_IR_SENSOR D5
#define PIN_MAGNET_SENSOR_TOP D6
#define PIN_MAGNET_SENSOR_BOTTOM D7

#define OBSTRUCTION_DELAY 45000
#define SENSOR_READ_DELAY 50

int topSensorValue = LOW;
int bottomSensorValue = LOW;
int irSensorValue = LOW;

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

// Low-level sensor reading:
bool isDoorFullyClosed() {
  bottomSensorValue = digitalRead(PIN_MAGNET_SENSOR_BOTTOM);
  Serial.print("doorFullyClosed: ");
  Serial.println(bottomSensorValue == LOW);
  return bottomSensorValue == LOW;
}

bool isDoorFullyOpen() {
  topSensorValue = digitalRead(PIN_MAGNET_SENSOR_TOP);
  Serial.print("doorFullyOpen: ");
  Serial.println(topSensorValue == LOW);
  return topSensorValue == LOW;
}

bool isDoorObstructed() {
  irSensorValue = digitalRead(PIN_IR_SENSOR);
  Serial.print("doorObstructed: ");
  Serial.println(irSensorValue == HIGH);
  return irSensorValue == HIGH;
}

// Low-level motor operation
void doorUp() {
  Serial.println("doorUp");
  digitalWrite(PIN_MOTOR_DOWN, LOW);
  digitalWrite(PIN_MOTOR_UP, HIGH);
}
void doorDown() {
  Serial.println("doorDown");
  digitalWrite(PIN_MOTOR_UP, LOW);
  digitalWrite(PIN_MOTOR_DOWN, HIGH);
}
void doorStop() {
  Serial.println("doorStop");
  digitalWrite(PIN_MOTOR_UP, LOW);
  digitalWrite(PIN_MOTOR_DOWN, LOW);
}

void openDoor() {
  blink(2);
  doorUp();
  for (;;) {
    if (isDoorFullyOpen()) {
      break;
    }
    delay(SENSOR_READ_DELAY);
  }
  doorStop();
}

// Logic
void closeDoor() {
  blink(3);
  doorDown();
  for (;;) {
    if (isDoorFullyClosed()) {
      break;
    }

    if (isDoorObstructed()) {
      blinkFast(5);
      doorStop();
      delay(200);
      openDoor();
      delay(OBSTRUCTION_DELAY);
      doorDown();
      continue;
    }
    delay(SENSOR_READ_DELAY);
  }
  doorStop();
}

void debugSensors() {
  ledOff();
  for (;;) {
    bool closed = isDoorFullyClosed();
    bool open = isDoorFullyOpen();
    bool obstructed = isDoorObstructed();
    Serial.print("IR: ");
    Serial.println(obstructed);
    Serial.print("OPEN: ");
    Serial.println(open);
    Serial.print("CLOSED: ");
    Serial.println(closed);
    if (open) {
      blink(1);
      delay(1000);
    } else if (closed) {
      blink(2);
      delay(1000);
    } else if (obstructed) {
      blink(3);
      delay(1000);
    } else {
      delay(SENSOR_READ_DELAY);
    }
    Serial.println();
  }
}

void testMotors() {
  for (;;) {
    ledOn();
    doorUp();
    delay(1000);
    doorStop();
    ledOff();
    delay(1000);

    ledOn();
    doorDown();
    delay(1000);
    doorStop();
    ledOff();
    delay(1000);
  }
}
