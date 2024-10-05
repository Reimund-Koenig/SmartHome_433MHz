#ifndef MQTTCONNECTOR
#define MQTTCONNECTOR
#include "constants/settings.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

class MQTTConnector {
  public:
    MQTTConnector(MQTT_CALLBACK_SIGNATURE);
    void publish(const char *topic, const char *message);
    void publish_433(const char *topic, unsigned long val, unsigned int bitLen);
    void subscribe(const char *topic); // Subscribes to a topic
    void loop();

  private:
    WiFiClient espClient;
    PubSubClient *client;
    void setup_wifi();
    void reconnect();
    bool isConnected();
};
#endif // MQTTCONNECTOR
