#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "door.h"
#include "config.h"

const char *mqtt_topic_config_open = "pites-cfg-open";
const char *mqtt_topic_config_close = "pites-cfg-close";
const char *mqtt_topic_action_open = "pites-act-open";
const char *mqtt_topic_action_close = "pites-act-close";
const char *mqtt_topic_status = "pites-status";
const char *mqtt_topic_status_opened = "pites-open";
const char *mqtt_topic_status_closed = "pites-close";

char clientID[11];
PubSubClient mqttClient;

void MQTTInit(WiFiClientSecure wifiClient, std::function<void (char *, uint8_t *, unsigned int)> callback)
{
  mqttClient = PubSubClient(wifiClient);
  sprintf(clientID, "pites-%ld", random(0xffff)); // Create a random client ID
  mqttClient.setServer(cfg_mqtt_server, cfg_mqtt_server_port);
  mqttClient.setCallback(callback);
}

void MQTTReconnect()
{
  // Loop until we're reconnected
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
  *output = hour*100 + minute;
  Serial.printf("Set %s time to %04u\n", openingOrClosing, *output);
}

void MQTTPublish(const char *topic, char *payload, boolean retained)
{
  if (mqttClient.publish(topic, payload, true))
  {
    Serial.printf("Message published [%s]: %s\n", topic, payload);
  }
}
