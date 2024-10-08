
#include "mqttconnector.h"
#include "constants/secrets.h" // rename and adapt secrets_template.h
#include <string>

MQTTConnector::MQTTConnector(MQTT_CALLBACK_SIGNATURE) {
    setup_wifi();
    if(WiFi.status() != WL_CONNECTED) { return; }
    client = new PubSubClient(espClient);
    client->setServer(MQTT_SERVER_IP_ADDRESS, MQTT_SERVER_PORT);
    client->setCallback(callback);
    reconnect();
};

void MQTTConnector::publish_433(const char *topic, unsigned long val) {
    std::string message = std::to_string(val);
    client->publish(topic, message.c_str());
}

void MQTTConnector::publish(const char *topic, const char *message) {
    client->publish(topic, message);
}

void MQTTConnector::loop() {
    if(WiFi.status() != WL_CONNECTED) { return; }
    if(client->connected()) {
        client->loop();
    } else {
        reconnect();
    }
}

void MQTTConnector::subscribe(const char *topic) { client->subscribe(topic); }

void MQTTConnector::setup_wifi() {
    delay(10);
    // Serial.println();
    // Serial.print("Connect to ");
    // Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int timeout = 10; // Seconds
    while(WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        timeout--;
        // Serial.print(".");
    }
    // Serial.println("");
    // Serial.println("WLAN connected");
    // Serial.print("IP address: ");
    // Serial.println(WiFi.localIP());
}

void MQTTConnector::reconnect() {
    if(WiFi.status() != WL_CONNECTED) { return; }
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Serial.println("Connect to MQTT...");
    if(client->connect(clientId.c_str())) {
        // Serial.println("connected");
        client->subscribe("home/433/learning");
    } else {
        // Serial.print("Fehler, rc=");
        // Serial.print(client->state());
        // Serial.println(" Wait 5 seconds and try again");
        delay(5000);
    }
}
