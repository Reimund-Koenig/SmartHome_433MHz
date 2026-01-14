#include "src/common/filesystem.h"
#include "src/common/fota.h"
#include "src/common/logger.h"
#include "src/common/mqttconnector.h"
#include <ESP.h>
#include <ESP8266WiFi.h>
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
unsigned long last_code;

int trigger_restart;

static const unsigned long WIFI_RUNTIME_DOWN_TIMEOUT_MS = 30000;
static unsigned long wifi_down_since_ms = 0;

MQTTConnector *mqtt = nullptr;
ControllerFileSystem *cfs = nullptr;
ControllerFota *controller_updater = nullptr;

struct MQTTMessage {
    String topic;
    String message;
};
const size_t MAX_MQTT_MESSAGES = 8;
const unsigned int MAX_PAYLOAD_LENGTH = 256;

std::vector<MQTTMessage> mqttMessages;
std::vector<String> subscriptions = {DEFAULT_MQTT_CHANNEL + "/learn"};

/* -------- Collision strip via A0 (minimal) ----------
   Rule:
   - Normal (8.2k) -> do nothing
   - If median(ADC) < 4 (debounced) -> publish collision
   - All other intermediate values -> ignore
   Debounced = median under threshold for N consecutive windows
*/
static const int COLLISION_ADC_PIN = A0;
static const int COLLISION_MEDIAN_SAMPLES = 9;
static const unsigned long COLLISION_SAMPLE_MS = 200;
static unsigned long next_collision_sample_ms = 0;
static const int COLLISION_THRESHOLD_RAW = 4;
static const int COLLISION_DEBOUNCE_WINDOWS = 3;
static int collision_under_cnt = 0;
static bool collision_latched = false;

void mqtt_callback(char *topic, byte *payload, unsigned int length);
void handle_mqtt_input(String topic, String msg);
void handle_mqtt();
void handle_heartbeat();
void handle_restart();
void check_receiver();
void check_learning();
void handler();

static void hard_restart_now(const char *reason) {
    if(serial_enabled) {
        Serial.println();
        Serial.print("!!! HARD RESTART: ");
        Serial.println(reason);
    }
    if(mqtt != nullptr) { mqtt->publish("/state", (String("NEUSTART: ") + reason).c_str()); }
    for(int i = 0; i < 10; i++) {
        delay(50);
    }
    ESP.restart();
    while(true) {
        delay(1);
    }
}

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

static int adc_median_9() {
    int a[COLLISION_MEDIAN_SAMPLES];
    for(int i = 0; i < COLLISION_MEDIAN_SAMPLES; i++) {
        a[i] = analogRead(COLLISION_ADC_PIN);
        delay(2);
    }
    for(int i = 1; i < COLLISION_MEDIAN_SAMPLES; i++) {
        int key = a[i];
        int j = i - 1;
        while(j >= 0 && a[j] > key) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
    return a[COLLISION_MEDIAN_SAMPLES / 2];
}

static void handle_collision_strip() {
    if(millis() < next_collision_sample_ms) return;
    next_collision_sample_ms = millis() + COLLISION_SAMPLE_MS;
    int med = adc_median_9();
    if(med < COLLISION_THRESHOLD_RAW) {
        if(collision_under_cnt < 1000) collision_under_cnt++;
    } else {
        collision_under_cnt = 0;
        collision_latched = false;
        return;
    }
    if(!collision_latched && collision_under_cnt >= COLLISION_DEBOUNCE_WINDOWS) {
        collision_latched = true;
        if(mqtt) mqtt->publish("/garage/collision", "1");
    }
}

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
    mqttMessages.reserve(MAX_MQTT_MESSAGES);
    mqtt = new MQTTConnector(mqtt_callback, DEFAULT_MQTT_CHANNEL, subscriptions, hard_restart_now);
    if(!mqtt) { hard_restart_now("MQTTConnector couldn't be initialized!"); }
    cfs = new ControllerFileSystem();
    if(!cfs) { hard_restart_now("ControllerFileSystem couldn't be initialized!"); }
    controller_updater = new ControllerFota(cfs, mqtt);
    if(!controller_updater) { hard_restart_now("ControllerFota couldn't be initialized!"); }

    last_code = 0;
    next_heartbeat = 0;
    next_accept_same_code = 0;
    trigger_restart = 0;
    learning_code = 0;

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

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    if(length > MAX_PAYLOAD_LENGTH) { return; }
    String topicStr = String(topic);
    String messageStr;
    for(unsigned int i = 0; i < length; i++) {
        messageStr += (char)payload[i];
    }
    mqttMessages.push_back({topicStr, messageStr});
    if(mqttMessages.size() >= MAX_MQTT_MESSAGES) { mqttMessages.erase(mqttMessages.begin()); }
}

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

void handle_mqtt() {
    while(!mqttMessages.empty()) {
        MQTTMessage msg = mqttMessages.front();
        handle_mqtt_input(msg.topic, msg.message);
        DEBUG_LOGLN("Topic: " + msg.topic + " - Message: " + msg.message);
        mqttMessages.erase(mqttMessages.begin());
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
    if(trigger_restart >= 25) { hard_restart_now("Manual restart trigger"); }
}

void handler() {
    seconds = (unsigned long)(millis() / 1000);
    wifi_runtime_watchdog_tick();
    handle_mqtt();
    handle_heartbeat();
    handle_restart();
    check_receiver();
    check_learning();
    handle_collision_strip();
}

void loop() {
    if(mqtt) mqtt->loop();
    handler();
    delay(2);
}
