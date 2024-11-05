
#include "mqttconnector.h"
#include "constants/secrets.h" // rename and adapt secrets_template.h
#include "logger.h"
#include <string>

MQTTConnector::MQTTConnector(MQTT_CALLBACK_SIGNATURE) {
    setup_wifi();
    if(WiFi.status() != WL_CONNECTED) { return; }
    client = new PubSubClient(espClient);
    client->setServer(MQTT_SERVER_IP_ADDRESS, MQTT_SERVER_PORT);
    client->setCallback(callback);
    reconnect();
};

void MQTTConnector::publish_433(unsigned long val) {
    const char *topic = "home/433";
    std::string message = std::to_string(val);
    client->publish(topic, message.c_str());
}

void MQTTConnector::publish(const char *topic, const char *message) {
    client->publish(topic, message);
}

void MQTTConnector::loop() {
    if(client->connected()) {
        client->loop();
    } else {
        reconnect();
    }
}

void MQTTConnector::subscribe(const char *topic) { client->subscribe(topic); }

void MQTTConnector::setup_wifi() {
    if(WiFi.status() == WL_CONNECTED) return;
    delay(10);
    DEBUG_PRINTLN();
    DEBUG_PRINT("Connect to ");
    DEBUG_PRINTLN(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int timeout = 10; // Seconds
    while(WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        timeout--;
        DEBUG_PRINT(".");
    }
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("WLAN connected");
    DEBUG_PRINT("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
}

void MQTTConnector::reconnect() {
    setup_wifi;
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    DEBUG_PRINTLN("Connect to MQTT...");
    if(client->connect(clientId.c_str())) {
        DEBUG_PRINTLN("connected");
        client->subscribe("home/433/learning");
    } else {
        DEBUG_PRINT("Fehler, rc=");
        DEBUG_PRINT(client->state());
        DEBUG_PRINTLN(" Wait 5 seconds and try again");
        delay(5000);
    }
}
