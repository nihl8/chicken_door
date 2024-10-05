// Thanks to https://github.com/aschiffler/esp8266-mqtt-node
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "common.h"
#include "config.h"
#include "door.h"

#define CERT mqtt_broker_cert
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

//--------------------------------------
// config (edit here before compiling)
//--------------------------------------
#define MQTT_TLS // uncomment this define to enable TLS transport

const char *mqtt_topic_config_open = "pites-cfg-open";
const char *mqtt_topic_config_close = "pites-cfg-close";
const char *mqtt_topic_action_open = "pites-act-open";
const char *mqtt_topic_action_close = "pites-act-close";
const char *mqtt_topic_status = "pites-status";
const char *mqtt_topic_opened = "pites-open";
const char *mqtt_topic_closed = "pites-close";

char *opening_label = "opening";
char *closing_label = "closing";

bool doorIsClosed;

#ifdef MQTT_TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "es.pool.ntp.org", 7200, 300);
PubSubClient mqttClient(wifiClient);

unsigned long lastStatusUpdateTime;
unsigned int openTime[2];
unsigned int closeTime[2];

void wifiSetup()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connected to Wi-Fi");

  if (WiFi.status() == WL_CONNECTED)
  {
    blinkFast(3);
    Serial.println("");
    Serial.print("Connected to #");
    Serial.print(ssid);
    Serial.print("# IP address: ");
    Serial.println(WiFi.localIP());
  }

  timeClient.begin();

#ifdef MQTT_TLS
  configTime(0, 0, "pool.ntp.org");
  X509List *cert = new X509List(mqtt_ca_cert);
  wifiClient.setTrustAnchors(cert);
#else
  wifiClient.setInsecure();
#endif

  Serial.println("WiFi connected");
}

char clientID[11];
void mqttInit()
{
  sprintf(clientID, "pites-%x", random(0xffff)); // Create a random client ID
  mqttClient.setServer(mqtt_server, mqtt_server_port);
  mqttClient.setCallback(mqttCallback);
}

void mqttReconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(clientID, mqtt_username, mqtt_password))
    {
      Serial.println("connected");

      mqttClient.subscribe(mqtt_topic_config_open);
      mqttClient.subscribe(mqtt_topic_config_close);
      mqttClient.subscribe(mqtt_topic_action_open);
      mqttClient.subscribe(mqtt_topic_action_close);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds"); // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void openTheDoor()
{
  Serial.println("Open sesame");
  // OpenDoor();
  doorIsClosed = false;
  mqttPublish(mqtt_topic_opened, "opened", false);
}

void closeTheDoor()
{
  Serial.println("Close sesame");
  // CloseDoor();
  doorIsClosed = true;
  mqttPublish(mqtt_topic_opened, "closed", false);
}

void parseIncomingDate(char *payload, unsigned int length, unsigned int *output, char *openingOrClosing)
{
  unsigned int payloadStrLength = strnlen(payload, 6);
  if (payloadStrLength != 5)
  {
    Serial.printf("Wrong payload length %d for %s time config!\n", payloadStrLength, openingOrClosing);
    return;
  }

  unsigned int hour, minute;
  sscanf(payload, "%2d:%2d", &hour, &minute);
  output[0] = hour;
  output[1] = minute;
  Serial.printf("Set %s time to %02u:%02u\n", openingOrClosing, hour, minute);
}

char incomingMessagePayload[1024];
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.println("");
  Serial.print("Message arrived on topic: '");
  Serial.print(topic);
  Serial.print("' with payload: ");
  for (unsigned int i = 0; i < length; i++)
  {
    incomingMessagePayload[i] = payload[i];
  }
  Serial.println(incomingMessagePayload);

  if (strcmp(topic, mqtt_topic_config_open) == 0)
  {
    parseIncomingDate(incomingMessagePayload, length, openTime, opening_label);
  }
  else if (strcmp(topic, mqtt_topic_config_close) == 0)
  {
    parseIncomingDate(incomingMessagePayload, length, closeTime, closing_label);
  }
  else if (strcmp(topic, mqtt_topic_action_open) == 0)
  {
    openTheDoor();
  }
  else if (strcmp(topic, mqtt_topic_action_close) == 0)
  {
    closeTheDoor;
  }
  // sendDoorStatus();
}

void mqttPublish(const char *topic, char *payload, boolean retained)
{
  if (mqttClient.publish(topic, payload, true))
  {
    Serial.printf("Message publised [%s]: %s\n", topic, payload);
  }
}

void setup()
{
  Serial.begin(74880);
  while (!Serial)
  {
    delay(1);
  }
  SetPinModes();

  Serial.println("");
  Serial.println("---START---");

  doorIsClosed = IsDoorFullyClosed();

  wifiSetup();
  mqttInit();
}

char status[1024];
void sendDoorStatus()
{
  sprintf(status,
          "{\"open\": %d,\"closed\": %d,\"obstructed\": %d, \"opensAt\": \"%02d:%02d\", \"closesAt\": \"%02d:%02d\", currentTime: \"%02d:%02d\"}",
          IsDoorFullyOpen(),
          IsDoorFullyClosed(),
          IsDoorObstructed(),
          openTime[0],
          openTime[1],
          closeTime[0],
          closeTime[1],
          timeClient.getHours(),
          timeClient.getMinutes());

  mqttPublish(mqtt_topic_status, status, true);
  lastStatusUpdateTime = timeClient.getEpochTime();
}

unsigned int currentHour, currentMinute;
void loop()
{
  //TODO:
  // - Use epoch times, and 
  // - Persist config
  //    https ://stackoverflow.com/questions/32616153/making-variables-persistent-after-a-restart-on-nodemcu
  //
  // - Deep sleep 
  //    Deep sleep mode for 30 seconds, the ESP8266 wakes up by itself when GPIO 16 (D0 in NodeMCU board) is connected to the RESET pin
  //    Serial.println("I'm awake, but I'm going into deep sleep mode for 30 seconds");
  //    ESP.deepSleep(30e6);

  if (lastStatusUpdateTime == 0 || timeClient.getEpochTime() - lastStatusUpdateTime >= 10)
  {
    sendDoorStatus();
  }
  currentHour = timeClient.getHours();
  currentMinute = timeClient.getMinutes();
/*  if (doorIsClosed)
  {
    if (currentHour > openTime[0] || currentHour == openTime[0] && currentMinute >= openTime[1])
    {
      OpenDoor();
    }
  }
  else
  {
    if (currentHour > openTime[0] || currentHour == openTime[0] && currentMinute >= openTime[1])
    {
      OpenDoor();
    }
  }
*/
  if (closeTime == 0 || timeClient.getEpochTime() - lastStatusUpdateTime >= 10)
  {
    sendDoorStatus();
  }

  delay(500);

  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  mqttClient.loop();
  timeClient.update();
}