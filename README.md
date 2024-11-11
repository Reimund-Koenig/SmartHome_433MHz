# SmartHome_433MHz

433MHz Sender and Receiver with Wemos D1 Mini to control my smart home

## How To Update over the Air

1. Open ./src/constants/settings.h and rename version to your need
2. Build the binary with Arduino IDE
3. Upload the binary
4. Change <http://reimund-koenig.de/data/smarthome/rf_current_version.info>
5. MQTT Publish via tut: mosquitto_pub -t "home/433/command" -m "restart"

## Hardware

* Wemos D1 Mini
* 433MHz Receiver Modul SRX882 with antenna
* 433MHz Transmitter Modul STX882 with antenna
