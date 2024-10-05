#include "src/constants/secrets.h"
#include "src/constants/settings.h"
#include <RCSwitch.h>

// init 433MHz lib
RCSwitch receiver = RCSwitch();
RCSwitch transmitter = RCSwitch();
uint32_t seconds;
uint32_t next_seconds;

struct Codes {
    unsigned long code;
    unsigned int bitLenght;
};

Codes *codes = NULL;
int numOfCodes = 0;

void setup() {
    Serial.begin(115200);
    pinMode(REICEIVER_DATA, INPUT);
    pinMode(TRANSMITTER_DATA, OUTPUT);
    pinMode(REICEIVER_SLEEP, OUTPUT);
    receiver.enableReceive(REICEIVER_DATA);       // 433MHz Receiver
    transmitter.enableTransmit(TRANSMITTER_DATA); // 433MHz Sender
    Serial.println("Sniffer for RF 433MHz");
    digitalWrite(REICEIVER_SLEEP, HIGH);
}

bool isInList(int code, int bitLenght) {
    for(int i = 0; i < numOfCodes; i++) {
        if(codes[i].code == code && codes[i].bitLenght == bitLenght) {
            return true; // Paar gefunden
        }
    }
    return false; // Paar nicht gefunden
}

void addToList(int code, int bitLenght) {
    if(isInList(code, bitLenght)) { return }
    Codes *temp = (Codes *)realloc(codes, (numOfCodes + 1) * sizeof(Codes));
    if(temp == NULL) {
        return; // Memory Violation
    }
    codes = temp;
    codes[numOfCodes].code = code;
    codes[numOfCodes].bitLenght = bitLenght;
    numOfCodes++;
}

void tick() {
    if(seconds == next_seconds) {
        Serial.println(seconds);
        next_seconds = seconds + 1;
        for(int i = 0; i < numOfCodes; i++) {
            Serial.print("Send Code: ");
            Serial.print(codes[i].code);
            Serial.print(" / ");
            Serial.print(codes[i].bitLenght);
            Serial.println("bit");
            transmitter.send(codes[i].code, codes[i].bitLenght);
        }
    }
    if(!receiver.available()) { return; }
    Serial.print(seconds);
    unsigned long tmp_code = receiver.getReceivedValue();
    unsigned int tmp_bitLenght = receiver.getReceivedBitlength();
    addToList(tmp_code, tmp_bitLenght);
    Serial.print("  - Received ");
    Serial.print(tmp_code);
    Serial.print(" / ");
    Serial.print(tmp_bitLenght);
    Serial.print("bit ");
    Serial.print("Protocol: ");
    Serial.println(receiver.getReceivedProtocol());
    receiver.resetAvailable();
}

void loop() {
    seconds = (int)(millis() / 1000);
    tick();
}
