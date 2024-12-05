// Thanks to https://github.com/aschiffler/esp8266-mqtt-node
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
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

const char *mqtt_topic_config_open = "cfg-open";
const char *mqtt_topic_config_close = "cfg-close";
const char *mqtt_topic_config_follow_the_sun = "cfg-fts";
const char *mqtt_topic_config_save = "cfg-save";
const char *mqtt_topic_action_operate = "operate";

const char *mqtt_topic_status = "status";
const char *mqtt_topic_status_movement = "movement";
const char *mqtt_topic_error = "error";

const char *opening_label = "opening";
const char *closing_label = "closing";

const char *opened_action_message = "{\"action\":\"opened\"}";
const char *closed_action_message = "{\"action\":\"closed\"}";

const char *open_label = "open";
const char *close_label = "close";

bool doorIsClosed;
bool wasClosedManually = false;
bool wasOpenedManually = false;

WiFiClientSecure wifiClient;
WiFiUDP ntpUDP;

time_t now;
struct tm *timeinfo;

PubSubClient mqttClient(wifiClient);

unsigned long currentEpoch;
unsigned long lastStatusUpdateTime;
unsigned long lastSunriseUpdateTime;

// Update intervals
unsigned int update_interval_sunrise = 7200;
unsigned int update_interval_status = 60;

// persistant config
unsigned int openTime = 0;
unsigned int closeTime = 0;
unsigned int sunsetTime = 0;
unsigned int sunriseTime = 0;
unsigned int followTheSun = 1;

#define EEPROM_SIZE 12

void timeSetup()
{
  configTime(0, 0, "es.pool.ntp.org");
  Serial.println("\nWaiting for time");
  while (true)
  {
    readTime();
    if (now > 170000000)
    { // Circa 1975
      break;
    }
    Serial.print("-");
    delay(1000);
  }
  Serial.println();
  printTime();
}

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
      mqttClient.subscribe(mqtt_topic_action_operate);
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
  publishEventMessage(mqtt_topic_status_movement, opened_action_message);
}

void closeTheDoor()
{
  Serial.println("Close sesame");
  CloseDoor();
  doorIsClosed = true;
  publishEventMessage(mqtt_topic_status_movement, closed_action_message);
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
  else if (strcmp(topic, mqtt_topic_action_operate) == 0)
  {
    if(strncmp("{\"action\":\"open\"}", incomingMessagePayload, length) == 0) {
      openTheDoor();
      wasOpenedManually = true;
      wasClosedManually = false;
    } else if (strncmp("{\"action\":\"close\"}", incomingMessagePayload, length) == 0) {
      closeTheDoor();
      wasClosedManually = true;
      wasOpenedManually = false;
    }
  }
  else if (strcmp(topic, mqtt_topic_config_follow_the_sun) == 0)
  {
    if (strncmp("{\"follow\":1}", incomingMessagePayload, length) == 0)
    {
      followTheSun = 1;
      lastSunriseUpdateTime = 0;
      lastStatusUpdateTime = 0; // Reset to force update on next loop
    }
    else if (strncmp("{\"follow\":0}", incomingMessagePayload, length) == 0)
    {
      followTheSun = 0;
    }
  }
  else if (strcmp(topic, mqtt_topic_config_save) == 0)
  {
    saveConfigToEEPROM();
  }
  sendDoorStatus();
}

void publishEventMessage(const char *topic, const char *action)
{
  char status[1024];
  JsonDocument doc;

  char fmtCurrentTime[6] = "";

  formatTime(currentTime(), fmtCurrentTime);

  doc["currentTime"] = fmtCurrentTime;
  doc["action"] = action;
  serializeJson(doc, status, 1024);
  mqttPublish(topic, status, false);
}

void publishErrorMessage(const char *topic, const char *error)
{
  char status[1024];
  JsonDocument doc;

  char fmtCurrentTime[6] = "";

  formatTime(currentTime(), fmtCurrentTime);

  doc["currentTime"] = fmtCurrentTime;
  doc["error"] = error;
  serializeJson(doc, status, 1024);
  mqttPublish(topic, status, false);
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

  wifiSetup();
  timeSetup();

  ArduinoOTA.setHostname(cfg_ota_hostname);
  ArduinoOTA.setPassword(cfg_ota_password);
  ArduinoOTA.begin();

  mqttInit();
}

unsigned int currentTime()
{
  readTime();
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
  doc["status"] = doorIsClosed ? close_label : open_label;
  doc["obstructed"] = IsDoorObstructed();
  serializeJson(doc, status, 1024);

  mqttPublish(mqtt_topic_status, status, true);
  lastStatusUpdateTime = getEpochTime();
}

void updateSunriseSunsetTime()
{
  lastSunriseUpdateTime = currentEpoch;
  Serial.print("lastSunriseUpdateTime: ");
  Serial.println(lastSunriseUpdateTime);

  if (QuerySunriseAndSunset(wifiClient, cfg_geo_lat, cfg_geo_lon, &sunriseTime, &sunsetTime) != 0)
  {
    Serial.println("[SUNRISE] Error: Could not update sunrise/sunset");
    publishErrorMessage(mqtt_topic_error, "Could not update sunrise/sunset");
  }
  else
  {
    Serial.printf("[SUNRISE] New sunrise: %04d, sunset: %04d\n", sunriseTime, sunsetTime);
  }
}

long getEpochTime()
{
  readTime();
  return now;
}

void mainLogic()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WARN: Not connected to wifi");
  }

  currentEpoch = getEpochTime();
  if (lastSunriseUpdateTime == 0 || currentEpoch - lastSunriseUpdateTime >= update_interval_sunrise)
  {
    updateSunriseSunsetTime();
    if (followTheSun)
    {
      openTime = sunriseTime;
      closeTime = sunsetTime;
    }
  }

  unsigned int now = currentTime();
  if (openTime > 0 && now >= openTime && now < closeTime)
  {
    if (wasOpenedManually)
    {
      wasOpenedManually = false;
    }
    if (doorIsClosed && !wasClosedManually)
    {
      openTheDoor();
    }
  }
  else if (closeTime > 0 && (now >= closeTime || now < openTime))
  {
    if (wasClosedManually)
    {
      wasClosedManually = false;
    }
    if (!doorIsClosed && !wasOpenedManually)
    {
      closeTheDoor();
    }
  }

  if (lastStatusUpdateTime == 0 || currentEpoch - lastStatusUpdateTime >= update_interval_status)
  {
    sendDoorStatus();
  }

  delay(2000);

  if (!mqttClient.connected())
  {
    mqttReconnect();
  }
  if (mqttClient.connected())
  {
    mqttClient.loop();
  }

  // TODO:
  //  - Deep sleep
  //     Deep sleep mode for 30 seconds, the ESP8266 wakes up by itself when GPIO 16 (D0 in NodeMCU board) is connected to the RESET pin
  //     Serial.println("I'm awake, but I'm going into deep sleep mode for 30 seconds");
  // Serial.println("Deep sleeping 5 minutes");
  // delay(500);
  // ESP.deepSleep(300e6); // 5 minutes
  // ESP.deepSleep(30e6); // 30 seconds
}

void readTime()
{
  time(&now);
  timeinfo = localtime(&now);
}

void printTime()
{
  char timePrintBuffer[80];
  Serial.print("The time is: ");
  strftime(timePrintBuffer, 80, "%d %B %Y %H:%M:%S", timeinfo);
  Serial.println(timePrintBuffer);
}

void loop()
{
  ArduinoOTA.handle();
  readTime();
  mainLogic();
  // DebugSensors();
  // TestMotors();
}