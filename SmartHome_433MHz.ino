#include "src/common/filesystem.h"
#include "src/common/fota.h"
#include "src/common/logger.h"
#include "src/common/mqttconnector.h"
#include <RCSwitch.h>
#include <ESP8266WiFi.h>
#include <ESP.h>

#ifdef DEBUG
bool serial_enabled = true;
#else
bool serial_enabled = false;
#endif

// -------------------- 433 MHz --------------------
RCSwitch receiver = RCSwitch();
RCSwitch transmitter = RCSwitch();

// -------------------- Timing / State --------------------
unsigned long seconds;
unsigned long next_heartbeat;
unsigned long learning_code;
unsigned long next_accept_same_code;
unsigned long last_code;

// Legacy restart trigger (still supported via command)
int trigger_restart;

// -------------------- Watchdogs --------------------
// WLAN runtime: reboot if 30s continuously down
static const unsigned long WIFI_RUNTIME_DOWN_TIMEOUT_MS = 30000;
static unsigned long wifi_down_since_ms = 0;

// -------------------- Controllers --------------------
MQTTConnector *mqtt = nullptr;
ControllerFileSystem *cfs = nullptr;
ControllerFota *controller_updater = nullptr;

// -------------------- MQTT RX queue --------------------
struct MQTTMessage {
    String topic;
    String message;
};

const size_t MAX_MQTT_MESSAGES = 8;
const unsigned int MAX_PAYLOAD_LENGTH = 256;

std::vector<MQTTMessage> mqttMessages;
std::vector<String> subscriptions = {DEFAULT_MQTT_CHANNEL + "/learn"};

// ------------------------- Forward declarations -------------------------
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void handle_mqtt_input(String topic, String msg);
void handle_mqtt();
void handle_heartbeat();
void handle_restart();
void check_receiver();
void check_learning();
void handler();

// -------------------- HARD restart callback for MQTTConnector --------------------
static void hard_restart_now(const char *reason) {
    if(serial_enabled) {
        Serial.println();
        Serial.print("!!! HARD RESTART: ");
        Serial.println(reason);
    }
    if(mqtt != nullptr) {
        mqtt->publish("/state", (String("NEUSTART: ") + reason).c_str());
    }
    for(int i = 0; i < 10; i++) {
        delay(50);
    }
    ESP.restart();
    // Fallback (should never reach)
    while(true) { delay(1); }
}

// -------------------- WiFi runtime watchdog --------------------
static void wifi_runtime_watchdog_tick() {
    if(WiFi.status() == WL_CONNECTED) {
        wifi_down_since_ms = 0;
        return;
    }
    if(wifi_down_since_ms == 0) {
        wifi_down_since_ms = millis();
        return;
    }
    if(millis() - wifi_down_since_ms >= WIFI_RUNTIME_DOWN_TIMEOUT_MS) {
        hard_restart_now("WLAN runtime down >= 30s");
    }
}

// ------------------------- Setup -------------------------
void setup() {
    if(serial_enabled) {
        Serial.begin(19200);
        unsigned long start = millis();
        while(!Serial && millis() - start < 5000) {
            delay(5);
        }
        DEBUG_LOGLN("Start Serial");
    }

    delay(500);
    mqttMessages.reserve(MAX_MQTT_MESSAGES); // Avoid repeated reallocations / fragmentation
    mqtt = new MQTTConnector(mqtt_callback, DEFAULT_MQTT_CHANNEL,
                             subscriptions, hard_restart_now);
    if(!mqtt) {
        hard_restart_now("MQTTConnector couldn't be initialized!");
    }
    cfs = new ControllerFileSystem();
    if(!cfs) {
        hard_restart_now("ControllerFileSystem couldn't be initialized!");
    }
    controller_updater = new ControllerFota(cfs, mqtt);
    if(!controller_updater) {
        hard_restart_now("ControllerFota couldn't be initialized!");
    }

    // Init state
    last_code = 0;
    next_heartbeat = 0;
    next_accept_same_code = 0;
    trigger_restart = 0;
    learning_code = 0;

    // Pins
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
    mqtt->publish("/state", "Started 433MHz controller");
}

// ------------------------- Learning / Receiver -------------------------
void check_learning() {
    if(learning_code == 0) { return; }
    mqtt->publish("/state", "Anlernen....");
    DEBUG_LOG("Start Learning Mode with Code: ");
    DEBUG_LOGLN(learning_code);
    for(int k = 0; k < 20; k++) {
        transmitter.send(learning_code, 24);
        delay(4); 
    }
    DEBUG_LOG("Finished Learning Mode with Code: ");
    DEBUG_LOGLN(learning_code);
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
        DEBUG_LOGLN(tmp_code);
        mqtt->publish(tmp_code);
        last_code = tmp_code;
    }
    receiver.resetAvailable();
}

// ------------------------- MQTT callback / RX queue -------------------------
void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    if(length > MAX_PAYLOAD_LENGTH) { return; }
    String topicStr = String(topic);
    String messageStr;
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

// ------------------------- MQTT input handling -------------------------
void handle_mqtt_input_commands(String topic, String msg) {
    String cmdTopic = DEFAULT_MQTT_CHANNEL + String("/command");
    if(topic != cmdTopic) { return; }
    if(msg == "restart") {
        mqtt->publish("/state", "ESP8266 wird neu gestartet...");
        DEBUG_LOGLN("NEUSTART (command)");
        trigger_restart = 1;
    }
}

void handle_mqtt_input_learn(String topic, String msg) {
    String learnTopic = DEFAULT_MQTT_CHANNEL + String("/learn");
    if(topic != learnTopic) { return; }
    mqtt->publish("/state", "Neuer Code wird gelernt");
    for(unsigned int i = 0; i < msg.length(); i++) {
        if(!isDigit(msg[i])) {
            mqtt->publish("/state", "Fehler im Code!");
            return;
        }
    }
    learning_code = strtoul(msg.c_str(), NULL, 10);
}

void handle_mqtt_input(String topic, String msg) {
    handle_mqtt_input_commands(topic, msg);
    handle_mqtt_input_learn(topic, msg);
}

// ------------------------- Heartbeat / Restart -------------------------
void handle_heartbeat() {
    if(seconds < next_heartbeat) return;
    mqtt->publish("/heartbeat", "alive");
    next_heartbeat = seconds + 30;
}

void handle_restart() {
    if(trigger_restart == 0) return;
    trigger_restart++;
    if(trigger_restart >= 25) {
        hard_restart_now("Manual restart trigger");
    }
}

// ------------------------- Main handler -------------------------
void handler() {
    seconds = (unsigned long)(millis() / 1000);
    // WiFi runtime watchdog: 30s down -> reboot
    wifi_runtime_watchdog_tick();
    handle_mqtt();
    handle_heartbeat();
    handle_restart();
    check_receiver();
    check_learning();
}

void loop() {
    if(mqtt) mqtt->loop(); 
    handler();
    delay(2);
}
