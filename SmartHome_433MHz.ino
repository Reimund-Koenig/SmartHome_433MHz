
// #define DEBUG
#include "src/logger.h"
#include "src/mqttconnector.h"
#include <RCSwitch.h>

#ifdef DEBUG
bool serial_enabled = true;
#else
bool serial_enabled = false;
#endif

// init 433MHz lib
RCSwitch receiver = RCSwitch();
RCSwitch transmitter = RCSwitch();
uint32_t seconds;
uint32_t next_heartbeat;
unsigned long learning_code;

WiFiClient espClient;
PubSubClient client(espClient);
MQTTConnector *mqtt;

struct Codes {
    unsigned long code;
    unsigned int bitLenght;
};

Codes *codes = NULL;
int numOfCodes = 0;

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    String message = "";
    for(int i = 0; i < length; i++) {
        char tmp = (char)payload[i];
        if(!isDigit(tmp)) { return; } // No integer
        message += tmp;
    }
    learning_code = strtoul(message.c_str(), NULL, 10);
}

void setup() {
    if(serial_enabled) {
        Serial.begin(19200);
        while(!Serial) {
            delay(1);
        }
    }
    next_heartbeat = 0;
    mqtt = new MQTTConnector(mqtt_callback);
    pinMode(REICEIVER_DATA, INPUT);
    pinMode(TRANSMITTER_DATA, OUTPUT);
    pinMode(TRANSMITTER_POWER, OUTPUT);
    pinMode(RECEIVER_POWER, OUTPUT);
    pinMode(REICEIVER_SLEEP, OUTPUT);
    receiver.enableReceive(REICEIVER_DATA);       // 433MHz Receiver
    transmitter.enableTransmit(TRANSMITTER_DATA); // 433MHz Sender
    digitalWrite(TRANSMITTER_POWER, HIGH);
    digitalWrite(RECEIVER_POWER, HIGH);
    digitalWrite(REICEIVER_SLEEP, HIGH);
    DEBUG_PRINTLN("Sniffer and sender for RF 433MHz");
}

bool isInList(int code, int bitLenght) {
    for(int i = 0; i < numOfCodes; i++) {
        if(codes[i].code == code && codes[i].bitLenght == bitLenght) {
            return true;
        }
    }
    return false;
}

void addToList(int code, int bitLenght) {
    if(isInList(code, bitLenght)) { return; }
    Codes *temp = (Codes *)realloc(codes, (numOfCodes + 1) * sizeof(Codes));
    if(temp == NULL) { return; } // Memory Violation
    codes = temp;
    codes[numOfCodes].code = code;
    codes[numOfCodes].bitLenght = bitLenght;
    numOfCodes++;
}

void resend_known_codes() {
    for(int i = 0; i < numOfCodes; i++) {
        DEBUG_PRINT("Send Code: ");
        DEBUG_PRINT(codes[i].code);
        DEBUG_PRINT(" / ");
        DEBUG_PRINT(codes[i].bitLenght);
        DEBUG_PRINTLN("bit");
        transmitter.send(codes[i].code, codes[i].bitLenght);
    }
}

void check_learning() {
    if(learning_code == 0) { return; }
    DEBUG_PRINT("Start Learning Mode with Code: ");
    DEBUG_PRINTLN(learning_code);
    for(int k = 0; k < 10; k++) {
        DEBUG_PRINTLN("Send Learning Code");
        transmitter.send(learning_code, 24);
        delay(10);
    }
    DEBUG_PRINT("Finished Learning Mode with Code: ");
    learning_code = 0;
}

void heartbeat() {
    if(seconds < next_heartbeat) return;
    mqtt->publish("home/433/heartbeat", "alive");
    DEBUG_PRINTLN(seconds);
    next_heartbeat = seconds + 30;
}

void check_receiver() {
    if(!receiver.available()) { return; }
    DEBUG_PRINT(seconds);
    unsigned long tmp_code = receiver.getReceivedValue();
    unsigned int tmp_bitLenght = receiver.getReceivedBitlength();
    unsigned int protocol = receiver.getReceivedProtocol();
    DEBUG_PRINT(" - Received: ");
    DEBUG_PRINT(tmp_code);
    DEBUG_PRINT(" / ");
    DEBUG_PRINT(tmp_bitLenght);
    DEBUG_PRINT("bit - ");
    DEBUG_PRINT("Protocol: ");
    DEBUG_PRINT(protocol);
    DEBUG_PRINT(" - Add to List - ");
    addToList(tmp_code, tmp_bitLenght);
    DEBUG_PRINT("MQTT Publish - ");
    mqtt->publish_433(tmp_code);
    DEBUG_PRINTLN(" - resetAvailable");
    receiver.resetAvailable();
}

void loop() {
    seconds = (int)(millis() / 1000);
    heartbeat();
    check_receiver();
    check_learning();
    mqtt->loop();
    delay(10);
}
