#include <Arduino.h>
#include "common.h"

#define PIN_MOTOR_UP 5
#define PIN_MOTOR_DOWN 4
#define PIN_IR_SENSOR 14
#define PIN_MAGNET_SENSOR_TOP 12
#define PIN_MAGNET_SENSOR_BOTTOM 13

#define OBSTRUCTION_DELAY 45000
#define SENSOR_READ_DELAY 50

int topSensorValue = LOW;
int bottomSensorValue = LOW;
int irSensorValue = LOW;

void SetPinModes()
{
  pinMode(PIN_MOTOR_UP, OUTPUT);
  pinMode(PIN_MOTOR_DOWN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(PIN_IR_SENSOR, INPUT_PULLUP);
  pinMode(PIN_MAGNET_SENSOR_TOP, INPUT_PULLUP);
  pinMode(PIN_MAGNET_SENSOR_BOTTOM, INPUT_PULLUP);
}

// Low-level sensor reading:
bool IsDoorFullyClosed()
{
  bottomSensorValue = digitalRead(PIN_MAGNET_SENSOR_BOTTOM);
  return bottomSensorValue == LOW;
}

bool IsDoorFullyOpen()
{
  topSensorValue = digitalRead(PIN_MAGNET_SENSOR_TOP);
  return topSensorValue == LOW;
}

bool IsDoorObstructed()
{
  irSensorValue = digitalRead(PIN_IR_SENSOR);
  return irSensorValue == HIGH;
}

// Low-level motor operation
void doorUp()
{
  Serial.println("doorUp");
  digitalWrite(PIN_MOTOR_DOWN, LOW);
  digitalWrite(PIN_MOTOR_UP, HIGH);
}
void doorDown()
{
  Serial.println("doorDown");
  digitalWrite(PIN_MOTOR_UP, LOW);
  digitalWrite(PIN_MOTOR_DOWN, HIGH);
}
void doorStop()
{
  Serial.println("doorStop");
  digitalWrite(PIN_MOTOR_UP, LOW);
  digitalWrite(PIN_MOTOR_DOWN, LOW);
}

void OpenDoor()
{
  blink(2);
  if (IsDoorFullyOpen())
  {
    return;
  }
  doorUp();
  for (;;)
  {
    if (IsDoorFullyOpen())
    {
      break;
    }
    delay(SENSOR_READ_DELAY);
  }
  doorStop();
}

// Logic
void CloseDoor()
{
  blink(3);
  if (IsDoorFullyClosed())
  {
    return;
  }
  doorDown();
  for (;;)
  {
    if (IsDoorFullyClosed())
    {
      break;
    }

    if (IsDoorObstructed())
    {
      blinkFast(5);
      doorStop();
      delay(200);
      OpenDoor();
      delay(OBSTRUCTION_DELAY);
      doorDown();
      continue;
    }
    delay(SENSOR_READ_DELAY);
  }
  doorStop();
}

void TebugSensors()
{
  ledOff();
  for (;;)
  {
    bool closed = IsDoorFullyClosed();
    bool open = IsDoorFullyOpen();
    bool obstructed = IsDoorObstructed();
    Serial.print("IR: ");
    Serial.println(obstructed);
    Serial.print("OPEN: ");
    Serial.println(open);
    Serial.print("CLOSED: ");
    Serial.println(closed);
    if (open)
    {
      blink(1);
      delay(1000);
    }
    else if (closed)
    {
      blink(2);
      delay(1000);
    }
    else if (obstructed)
    {
      blink(3);
      delay(1000);
    }
    else
    {
      delay(SENSOR_READ_DELAY);
    }
    Serial.println();
  }
}

void TestMotors()
{
  for (;;)
  {
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
