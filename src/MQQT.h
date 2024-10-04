#include <PubSubClient.h>

// Broker details
const char* mqtt_broker = "1e09fac79960414bbd2fea1b55f9a25f.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "pites";
const char* mqtt_password = "ESP32arduinopitespower4001==";

// Topic for IR sensor
const char* topic_sub_config = "chooks/config";
const char* topic_pub_status = "chooks/status";

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

void setupMQTT() {
  mqttClient.setServer(mqtt_broker, mqtt_port);
}

void reconnect() {
  Serial.println("Connecting to MQTT Broker...");
  while (!mqttClient.connected()) {
    Serial.println("Reconnecting to MQTT Broker...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT Broker.");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}