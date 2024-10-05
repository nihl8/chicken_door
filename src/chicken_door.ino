// Thanks to https://github.com/aschiffler/esp8266-mqtt-node
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include "common.h"
#include "config.h"
#include "door.h"
#include "sunrise.h"

#define CERT mqtt_broker_cert
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

#define _USE_EEPROM

const char *mqtt_topic_config_open = "pites-cfg-open";
const char *mqtt_topic_config_close = "pites-cfg-close";
const char *mqtt_topic_follow_the_sun = "pites-cfg-fts";
const char *mqtt_topic_action_open = "pites-act-open";
const char *mqtt_topic_action_close = "pites-act-close";
const char *mqtt_topic_status = "pites-status";
const char *mqtt_topic_opened = "pites-open";
const char *mqtt_topic_closed = "pites-close";

char *opening_label = "opening";
char *closing_label = "closing";

bool doorIsClosed;

WiFiClientSecure wifiClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "es.pool.ntp.org", 7200, 300);
PubSubClient mqttClient(wifiClient);

unsigned long currentEpoch;
unsigned long lastStatusUpdateTime;
unsigned long lastSunriseUpdateTime;
// times
unsigned int openTime = 0;
unsigned int closeTime = 0;
unsigned int sunsetTime = 0;
unsigned int sunriseTime = 0;
// other config
unsigned int followTheSun = 1;

#define EEPROM_SIZE 12

void wifiSetup()
{
  WiFi.begin(cfg_wifi_ssid, cfg_wifi_password);
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
    Serial.print(cfg_wifi_ssid);
    Serial.print("# IP address: ");
    Serial.println(WiFi.localIP());
  }

  timeClient.begin();

  configTime(0, 0, "es.pool.ntp.org");
  X509List *certs = new X509List(mqtt_ca_cert);
  certs->append(sunset_api_ca_cert);
  wifiClient.setTrustAnchors(certs);

  Serial.println("WiFi connected");
}

char clientID[11];
void mqttInit()
{
  sprintf(clientID, "pites-%lx", random(0xffff)); // Create a random client ID
  mqttClient.setServer(cfg_mqtt_server, cfg_mqtt_server_port);
  mqttClient.setCallback(mqttCallback);
}

void mqttReconnect()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(clientID, cfg_mqtt_username, cfg_mqtt_password))
    {
      Serial.println("connected");

      mqttClient.subscribe(mqtt_topic_config_open);
      mqttClient.subscribe(mqtt_topic_config_close);
      mqttClient.subscribe(mqtt_topic_action_open);
      mqttClient.subscribe(mqtt_topic_action_close);
      mqttClient.subscribe(mqtt_topic_follow_the_sun);
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
  *output = hour * 100 + minute;
  Serial.printf("Set %s time to %04u\n", openingOrClosing, *output);
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
    parseIncomingDate(incomingMessagePayload, length, &openTime, opening_label);
    followTheSun = 0;
  }
  else if (strcmp(topic, mqtt_topic_config_close) == 0)
  {
    parseIncomingDate(incomingMessagePayload, length, &closeTime, closing_label);
    followTheSun = 0;
  }
  else if (strcmp(topic, mqtt_topic_action_open) == 0)
  {
    openTheDoor();
  }
  else if (strcmp(topic, mqtt_topic_follow_the_sun) == 0)
  {
    if(strncmp("1", incomingMessagePayload, length)) {
      followTheSun = 1;
      lastStatusUpdateTime = 0; // Reset to force update on next loop
    } else {
      followTheSun = 0;
    }
  }
  else if (strcmp(topic, mqtt_topic_action_close) == 0)
  {
    closeTheDoor;
  }
  sendDoorStatus();
}

void mqttPublish(const char *topic, char *payload, boolean retained)
{
  if (mqttClient.publish(topic, payload, true))
  {
    Serial.printf("Message published [%s]: %s\n", topic, payload);
  }
}

void loadConfigFromEEPROM() {

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

  #ifdef _USE_EEPROM
  // EEPROM.begin(EEPROM_SIZE);
  loadConfigFromEEPROM();
  #endif

  wifiSetup();
  mqttInit();
}

// void formatTime(unsigned int timeInt, char* out) {
//   sprintf(out, "%02d:%02d", timeInt/100, timeInt%100);
// }

unsigned int currentTime()
{
  return timeClient.getHours() * 100 + timeClient.getMinutes();
}

void sendDoorStatus()
{
  char status[1024];
  JsonDocument doc;
  doc["open"] = IsDoorFullyOpen();
  doc["closed"] = IsDoorFullyClosed();
  doc["obstructed"] = IsDoorObstructed();
  doc["opensAt"] = openTime;
  doc["closesAt"] = closeTime;
  doc["currentTime"] = currentTime();
  doc["sunset"] = sunriseTime;
  doc["sunrise"] = sunsetTime;
  doc["followTheSun"] = followTheSun;
  serializeJson(doc, status, 1024);

  mqttPublish(mqtt_topic_status, status, true);
  lastStatusUpdateTime = timeClient.getEpochTime();
}

void updateSunriseSunsetTime()
{
  lastSunriseUpdateTime = currentEpoch;
  if (QuerySunriseAndSunset(wifiClient, cfg_geo_lat, cfg_geo_lon, &sunriseTime, &sunsetTime) != 0)
  {
    Serial.println("Error: Could not update sunrise/sunset");
  }
  else
  {
    Serial.printf("New sunrise: %04d, sunset: %04d\n", sunriseTime, sunsetTime);
  }
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WARN: Not connected to wifi");
  }

  // TODO:
  //  - Use epoch times
  //  - EEPROM persist every week
  //     https://www.aranacorp.com/en/using-the-eeprom-with-the-esp8266/
  //
  //  - Deep sleep
  //     Deep sleep mode for 30 seconds, the ESP8266 wakes up by itself when GPIO 16 (D0 in NodeMCU board) is connected to the RESET pin
  //     Serial.println("I'm awake, but I'm going into deep sleep mode for 30 seconds");
  //     ESP.deepSleep(30e6);

  currentEpoch = timeClient.getEpochTime();
  if (followTheSun == 1 && (lastSunriseUpdateTime == 0 || currentEpoch - lastSunriseUpdateTime >= 10))
  {
    updateSunriseSunsetTime();
  }
  if (lastStatusUpdateTime == 0 || currentEpoch - lastStatusUpdateTime >= 10)
  {
    sendDoorStatus();
  }

  unsigned int now = currentTime();
  if (doorIsClosed && openTime > 0 && now >= openTime)
  {
    openTheDoor();
  }
  else if (!doorIsClosed && closeTime > 0 && now >= closeTime)
  {
    closeTheDoor();
  }

  delay(500);

  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  mqttClient.loop();
  timeClient.update();
}