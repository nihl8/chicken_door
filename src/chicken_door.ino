// Thanks to https://github.com/aschiffler/esp8266-mqtt-node
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Dangerous bug, don't use: https://github.com/arduino-libraries/NTPClient/issues/105
// #include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <time.h>
#include <sys/time.h>

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
const char *mqtt_topic_config_follow_the_sun = "pites-cfg-fts";
const char *mqtt_topic_config_save = "pites-cfg-save";
const char *mqtt_topic_action_open = "pites-act-open";
const char *mqtt_topic_action_close = "pites-act-close";
const char *mqtt_topic_status = "pites-status";
const char *mqtt_topic_status_opened = "pites-opened";
const char *mqtt_topic_status_closed = "pites-closed";

const char *opening_label = "opening";
const char *closing_label = "closing";

const char *opened_label = "opened";
const char *closed_label = "closed";

bool doorIsClosed;

WiFiClientSecure wifiClient;
WiFiUDP ntpUDP;

timeval tv;
time_t now;
struct tm * timeinfo;

PubSubClient mqttClient(wifiClient);

unsigned long currentEpoch;
unsigned long lastStatusUpdateTime;
unsigned long lastSunriseUpdateTime;
// persistant config
unsigned int openTime = 0;
unsigned int closeTime = 0;
unsigned int sunsetTime = 0;
unsigned int sunriseTime = 0;
unsigned int followTheSun = 1;

#define EEPROM_SIZE 12

void wifiSetup()
{
  WiFi.begin(cfg_wifi_ssid, cfg_wifi_password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

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

  configTime(0, 0, "es.pool.ntp.org");
  // X509List *certs = new X509List(mqtt_ca_cert);
  // certs->append(sunset_api_ca_cert);
  // wifiClient.setTrustAnchors(certs);
  wifiClient.setInsecure();

  Serial.println("WiFi connected");
}

char clientID[11];
void mqttInit()
{
  sprintf(clientID, "pites-%lx", random(0xffff)); // Create a random client ID
  mqttClient.setServer(cfg_mqtt_server, cfg_mqtt_server_port);
  mqttClient.setCallback(mqttCallback);
  mqttReconnect();
}

bool mqttReconnect()
{
  for (unsigned int attempts = 1; !mqttClient.connected() && attempts <= 3; attempts++)
  {
    // Attempt to connect
    if (mqttClient.connect(clientID, cfg_mqtt_username, cfg_mqtt_password))
    {
      Serial.println("[MQTT] Connected");

      mqttClient.subscribe(mqtt_topic_config_open);
      mqttClient.subscribe(mqtt_topic_config_close);
      mqttClient.subscribe(mqtt_topic_action_open);
      mqttClient.subscribe(mqtt_topic_action_close);
      mqttClient.subscribe(mqtt_topic_config_follow_the_sun);
      mqttClient.subscribe(mqtt_topic_config_save);
      return true;
    }
    else
    {
      Serial.print("[MQTT] Connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds"); // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  return false;
}

void openTheDoor()
{
  Serial.println("Open sesame");
  OpenDoor();
  doorIsClosed = false;
  mqttPublish(mqtt_topic_status_opened, strdup(opened_label), false);
}

void closeTheDoor()
{
  Serial.println("Close sesame");
  CloseDoor();
  doorIsClosed = true;
  mqttPublish(mqtt_topic_status_closed, strdup(closed_label), false);
}

void parseIncomingDate(char *payload, unsigned int length, unsigned int *output, const char *openingOrClosing)
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
  Serial.print("[MQTT] Message arrived on topic: '");
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
  else if (strcmp(topic, mqtt_topic_config_follow_the_sun) == 0)
  {
    if (strncmp("1", incomingMessagePayload, length))
    {
      followTheSun = 1;
      lastStatusUpdateTime = 0; // Reset to force update on next loop
    }
    else
    {
      followTheSun = 0;
    }
  }
  else if (strcmp(topic, mqtt_topic_config_save) == 0)
  {
    saveConfigToEEPROM();
  }
  else if (strcmp(topic, mqtt_topic_action_close) == 0)
  {
    closeTheDoor();
  }
  sendDoorStatus();
}

void mqttPublish(const char *topic, char *payload, boolean retained)
{
  if (mqttClient.publish(topic, payload, true))
  {
    Serial.printf("[MQTT] Message published [%s]: %s\n", topic, payload);
  }
}

void saveConfigToEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);
  int address = 0;

  EEPROM.put(address, openTime);
  address += sizeof(openTime);

  EEPROM.put(address, closeTime);
  address += sizeof(closeTime);

  EEPROM.put(address, sunsetTime);
  address += sizeof(sunsetTime);

  EEPROM.put(address, sunriseTime);
  address += sizeof(sunriseTime);

  EEPROM.put(address, followTheSun);
  address += sizeof(followTheSun);

  EEPROM.commit();

  Serial.println("Saved config to EEPROM");
  EEPROM.end();
}

void loadConfigFromEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);
  int address = 0;

  EEPROM.get(address, openTime);
  address += sizeof(openTime);

  EEPROM.get(address, closeTime);
  address += sizeof(closeTime);

  EEPROM.get(address, sunsetTime);
  address += sizeof(sunsetTime);

  EEPROM.get(address, sunriseTime);
  address += sizeof(sunriseTime);

  EEPROM.get(address, followTheSun);
  address += sizeof(followTheSun);

  Serial.println("Loaded config from EEPROM");
  EEPROM.end();
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

  EEPROM.begin(EEPROM_SIZE);
  loadConfigFromEEPROM();

  configTime(0, 0, "es.pool.ntp.org");

  wifiSetup();
  mqttInit();
}

unsigned int currentTime()
{
  time(&now);
  timeinfo = localtime(&now);
  return timeinfo->tm_hour * 100 + timeinfo->tm_min;
}

void formatTime(int intDate, char *strDate)
{
  sprintf(strDate, "%02d:%02d", intDate / 100, intDate % 100);
}

void sendDoorStatus()
{
  char status[1024];
  JsonDocument doc;

  char fmtOpenTime[6] = "";
  char fmtCloseTime[6] = "";
  char fmtCurrentTime[6] = "";
  char fmtSunriseTime[6] = "";
  char fmtSunsetTime[6] = "";

  formatTime(openTime, fmtOpenTime);
  formatTime(closeTime, fmtCloseTime);
  formatTime(currentTime(), fmtCurrentTime);
  formatTime(sunriseTime, fmtSunriseTime);
  formatTime(sunsetTime, fmtSunsetTime);

  doc["currentTime"] = fmtCurrentTime;
  doc["opensAt"] = fmtOpenTime;
  doc["closesAt"] = fmtCloseTime;
  doc["followTheSun"] = followTheSun;
  doc["sunrise"] = fmtSunriseTime;
  doc["sunset"] = fmtSunsetTime;
  doc["closed"] = doorIsClosed;
  doc["obstructed"] = IsDoorObstructed();
  serializeJson(doc, status, 1024);

  mqttPublish(mqtt_topic_status, status, true);
  lastStatusUpdateTime = getEpochTime();
}

void updateSunriseSunsetTime()
{
  lastSunriseUpdateTime = currentEpoch;
  if (QuerySunriseAndSunset(wifiClient, cfg_geo_lat, cfg_geo_lon, &sunriseTime, &sunsetTime) != 0)
  {
    Serial.println("[SUNRISE] Error: Could not update sunrise/sunset");
  }
  else
  {
    Serial.printf("[SUNRISE] New sunrise: %04d, sunset: %04d\n", sunriseTime, sunsetTime);
  }
}

long getEpochTime() {
  return gettimeofday(&tv, nullptr);
  return tv.tv_sec;
}

void mainLogic()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WARN: Not connected to wifi");
    blink(1);
  }
  // else
  // {
  //   blinkFast(2);
  // }

  // TODO:
  //  - Deep sleep
  //     Deep sleep mode for 30 seconds, the ESP8266 wakes up by itself when GPIO 16 (D0 in NodeMCU board) is connected to the RESET pin
  //     Serial.println("I'm awake, but I'm going into deep sleep mode for 30 seconds");
  //     ESP.deepSleep(30e6);

  currentEpoch = getEpochTime();
  if (lastSunriseUpdateTime == 0 || currentEpoch - lastSunriseUpdateTime >= 7200)
  {
    updateSunriseSunsetTime();
    if (followTheSun)
    {
      openTime = sunriseTime;
      closeTime = sunsetTime;
    }
  }
  if (lastStatusUpdateTime == 0 || currentEpoch - lastStatusUpdateTime >= 20)
  {
    sendDoorStatus();
  }

  unsigned int now = currentTime();
  if (doorIsClosed && openTime > 0 && now >= openTime && now < closeTime)
  {
    openTheDoor();
  }
  else if (!doorIsClosed && closeTime > 0 && (now >= closeTime || now < openTime))
  {
    closeTheDoor();
  }

  delay(500);

  if (!mqttClient.connected())
  {
    mqttReconnect();
  }
  if (mqttClient.connected())
  {
    mqttClient.loop();
  }

}

void loop() { 
  mainLogic();
  // DebugSensors();
  // TestMotors();
}