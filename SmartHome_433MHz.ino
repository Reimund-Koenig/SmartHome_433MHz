
#include "src/common/filesystem.h"
#include "src/common/fota.h"
#include "src/common/logger.h"
#include "src/common/mqttconnector.h"
#include <RCSwitch.h>

#ifdef DEBUG
bool serial_enabled = true;
#else
bool serial_enabled = false;
#endif

RCSwitch receiver = RCSwitch();
RCSwitch transmitter = RCSwitch();
unsigned long seconds;
unsigned long next_heartbeat;
unsigned long learning_code;
unsigned long next_receiver;
int trigger_restart;

WiFiClient espClient;
PubSubClient client(espClient);
MQTTConnector *mqtt;
ControllerFileSystem *cfs;
ControllerFota *controller_updater;

struct MQTTMessage {
    String topic;
    String message;
};

std::vector<MQTTMessage> mqttMessages;

int numOfCodes = 0;
void setup() {
    if(serial_enabled) {
        Serial.begin(19200);
        while(!Serial) {
            delay(1);
        }
        delay(1000);
    }
    mqtt = new MQTTConnector(mqtt_callback, DEFAULT_MQTT_CHANNEL);
    cfs = new ControllerFileSystem();
    controller_updater = new ControllerFota(cfs, mqtt);
    next_heartbeat = 0;
    next_receiver = 0;
    trigger_restart = 0;
    pinMode(REICEIVER_DATA, INPUT);
    pinMode(TRANSMITTER_DATA, OUTPUT);
    pinMode(TRANSMITTER_POWER, OUTPUT);
    pinMode(RECEIVER_POWER, OUTPUT);
    pinMode(REICEIVER_SLEEP, OUTPUT);
    receiver.enableReceive(REICEIVER_DATA);
    transmitter.enableTransmit(TRANSMITTER_DATA);
    digitalWrite(TRANSMITTER_POWER, HIGH);
    digitalWrite(RECEIVER_POWER, HIGH);
    digitalWrite(REICEIVER_SLEEP, HIGH);
    DEBUG_LOGLN("Sniffer and sender for RF 433MHz");
}

void check_learning() {
    if(learning_code == 0) { return; }
    DEBUG_LOG("Start Learning Mode with Code: ");
    DEBUG_LOGLN(learning_code);
    for(int k = 0; k < 100; k++) {
        DEBUG_LOGLN("Send Learning Code");
        transmitter.send(learning_code, 24);
        delay(2);
    }
    DEBUG_LOG("Finished Learning Mode with Code: ");
    learning_code = 0;
}

void check_receiver() {
    if(millis() < next_receiver) return;
    if(!receiver.available()) { return; }
    next_receiver = millis() + 1000;
    unsigned long tmp_code = receiver.getReceivedValue();
    unsigned int tmp_bitLenght = receiver.getReceivedBitlength();
    unsigned int protocol = receiver.getReceivedProtocol();
    mqtt->publish(tmp_code);
    receiver.resetAvailable();
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    String topicStr = String(topic);
    String messageStr;
    for(unsigned int i = 0; i < length; i++) {
        messageStr += (char)payload[i];
    }
    mqttMessages.push_back({topicStr, messageStr});
}

void handle_mqtt() {
    while(!mqttMessages.empty()) {
        MQTTMessage msg = mqttMessages.front();
        handle_mqtt_input(msg.topic, msg.message);
        DEBUG_LOGLN("Topic: " + msg.topic + " - Message: " + msg.message);
        mqttMessages.erase(mqttMessages.begin());
    }
}

void handle_mqtt_input(String topic, String msg) {
    if(topic != "home/433/command") { return; }
    if(msg == "restart") {
        mqtt->publish("/state", "ESP32 wird neu gestartet...");
        DEBUG_LOGLN("NEUSTART");
        trigger_restart = 1;
    } else if(msg == "learn") {
        for(unsigned int i = 0; i < msg.length(); i++) {
            if(!isDigit(msg[i])) { return; }
        }
        learning_code = strtoul(msg.c_str(), NULL, 10);
    }
}

void handle_heartbeat() {
    if(seconds < next_heartbeat) return;
    mqtt->publish("/heartbeat", "alive");
    next_heartbeat = seconds + 30;
}

void handle_restart() {
    if(trigger_restart == 0) return;
    trigger_restart++;
    if(trigger_restart >= 25) { ESP.restart(); }
}

void handler() {
    seconds = (unsigned long)(millis() / 1000);
    handle_mqtt();
    handle_heartbeat();
    handle_restart();
    check_receiver();
    check_learning();
}

void loop() {
    mqtt->loop();
    handler();
    delay(2);
}
