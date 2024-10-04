#include <ESP8266WiFi.h>
#include "Door.h"
#include "wifi_config.h"

void ConnectToWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi.mode(WIFI_STA)");

  Serial.print("trying to connect to #");
  Serial.print(ssid);
  Serial.println("#");
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    yield();
    blink(1);

    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED ) {
    blinkFast(3);
    Serial.println("");
    Serial.print("Connected to #");
    Serial.print(ssid);
    Serial.print("# IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup() {
  Serial.begin(9600);
  // put your setup code here, to run once:
  pinMode(PIN_MOTOR_UP, OUTPUT);
  pinMode(PIN_MOTOR_DOWN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(PIN_IR_SENSOR, INPUT_PULLUP);
  pinMode(PIN_MAGNET_SENSOR_TOP, INPUT_PULLUP);
  pinMode(PIN_MAGNET_SENSOR_BOTTOM, INPUT_PULLUP);

  ConnectToWiFi();
  
  delay(15000);
}

void loop() {
  ledOff();
  // isDoorObstructed();
  // debugSensors();
  // testMotors();

  //  openDoor();

  // closeDoor();
  // delay(1000);

  blinkFast(1);
  delay(5000);
}