
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
unsigned long next_accept_same_code;
int trigger_restart;
unsigned long last_code;

WiFiClient espClient;
PubSubClient client(espClient);
MQTTConnector *mqtt;
ControllerFileSystem *cfs;
ControllerFota *controller_updater;

struct MQTTMessage {
    String topic;
    String message;
};
const size_t MAX_MQTT_MESSAGES = 8;
const unsigned int MAX_PAYLOAD_LENGTH = 256;

std::vector<MQTTMessage> mqttMessages;
std::vector<String> subscriptions = {DEFAULT_MQTT_CHANNEL + "/learn"};

int numOfCodes = 0;
void setup() {
    if(serial_enabled) {
        Serial.begin(19200);
        unsigned long start = millis();
        while(!Serial && millis() - start < 5000) {
            delay(5);
        }
        DEBUG_LOGLN("Start Serial and Watchdog");
    }

    delay(1000);
    try {
        mqtt = new MQTTConnector(mqtt_callback, DEFAULT_MQTT_CHANNEL,
                                 subscriptions);
        DEBUG_LOGLN("MQTTConnector initialized!");
    } catch(const std::bad_alloc &e) {
        DEBUG_LOGLN("MQTTConnector couldn't be initialized!");
        while(true) {}
    }
    try {
        cfs = new ControllerFileSystem();
        DEBUG_LOGLN("ControllerFileSystem initialized!");
    } catch(const std::bad_alloc &e) {
        DEBUG_LOGLN("ControllerFileSystem couldn't be initialized!");
        while(true) {}
    }
    try {
        controller_updater = new ControllerFota(cfs, mqtt);
        DEBUG_LOGLN("Start controller_updater");
    } catch(const std::bad_alloc &e) {
        DEBUG_LOGLN("controller_updater couldn't be initialized!");
        while(true) {}
    }
    DEBUG_LOGLN("Start Pins");
    last_code = 0;
    next_heartbeat = 0;
    next_accept_same_code = 0;
    trigger_restart = 0;
    pinMode(REICEIVER_DATA, INPUT);
    pinMode(TRANSMITTER_DATA, OUTPUT);
    pinMode(TRANSMITTER_POWER, OUTPUT);
    pinMode(RECEIVER_POWER, OUTPUT);
    pinMode(REICEIVER_SLEEP, OUTPUT);
    pinMode(TRANSMITTER_GND, OUTPUT);
    digitalWrite(TRANSMITTER_GND, LOW);
    digitalWrite(TRANSMITTER_POWER, HIGH);
    digitalWrite(RECEIVER_POWER, HIGH);
    digitalWrite(REICEIVER_SLEEP, HIGH);
    delay(250);
    receiver.enableReceive(REICEIVER_DATA);
    transmitter.enableTransmit(TRANSMITTER_DATA);
    DEBUG_LOGLN("Sniffer and sender for RF 433MHz");
}

void check_learning() {
    if(learning_code == 0) { return; }
    mqtt->publish("/state", "Anlernen....");
    DEBUG_LOG("Start Learning Mode with Code: ");
    DEBUG_LOGLN(learning_code);
    for(int k = 0; k < 20; k++) {
        DEBUG_LOGLN("Send Learning Code");
        transmitter.send(learning_code, 24);
        delay(4);
    }
    DEBUG_LOG("Finished Learning Mode with Code: ");
    learning_code = 0;
    mqtt->publish("/state", "Anlernen beendet");
}

void check_receiver() {
    if(millis() > next_accept_same_code) {
        next_accept_same_code = millis() + 5000;
        last_code = 0;
    }
    if(!receiver.available()) { return; }
    unsigned long tmp_code = receiver.getReceivedValue();
    if(tmp_code != last_code || last_code == 0) {
        // unsigned int tmp_bitLenght = receiver.getReceivedBitlength();
        // unsigned int protocol = receiver.getReceivedProtocol();
        DEBUG_LOGLN(tmp_code);
        mqtt->publish(tmp_code);
        last_code = tmp_code;
    }
    receiver.resetAvailable();
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    String topicStr = String(topic);
    String messageStr;
    if(length > MAX_PAYLOAD_LENGTH) { return; }
    for(unsigned int i = 0; i < length; i++) {
        messageStr += (char)payload[i];
    }
    mqttMessages.push_back({topicStr, messageStr});
    if(mqttMessages.size() >= MAX_MQTT_MESSAGES) {
        mqttMessages.erase(mqttMessages.begin());
    }
}

void handle_mqtt() {
    while(!mqttMessages.empty()) {
        MQTTMessage msg = mqttMessages.front();
        handle_mqtt_input(msg.topic, msg.message);
        DEBUG_LOGLN("Topic: " + msg.topic + " - Message: " + msg.message);
        mqttMessages.erase(mqttMessages.begin());
    }
}

void handle_mqtt_input_commands(String topic, String msg) {
    if(topic != "home/433/command") { return; }
    if(msg == "restart") {
        mqtt->publish("/state", "ESP32 wird neu gestartet...");
        DEBUG_LOGLN("NEUSTART");
        trigger_restart = 1;
    }
}

void handle_mqtt_input_learn(String topic, String msg) {
    if(topic != "home/433/learn") { return; }
    mqtt->publish("/state", "Neuer Code wird gelernt");
    for(unsigned int i = 0; i < msg.length(); i++) {
        if(!isDigit(msg[i])) {
            mqtt->publish("/state", "    Fehler im Code!");
            return;
        }
    }
    learning_code = strtoul(msg.c_str(), NULL, 10);
}

void handle_mqtt_input(String topic, String msg) {
    handle_mqtt_input_commands(topic, msg);
    handle_mqtt_input_learn(topic, msg);
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
    // ESP.wdtFeed();
    mqtt->loop();
    handler();
    delay(2);
}
