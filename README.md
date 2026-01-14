# SmartHome_433MHz

433MHz Sender and Receiver with Wemos D1 Mini to control my smart home

## Libs Needed

* RC-Switch by sui77 (v2.6.4)
* PubSubClient by Neal O'Leary (v2.8)

## Compile

### Add needed compiler flags

* Open Windows: C:\Users\<user>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\<version>\platform.txt
  * On Mac/Linux: ~/Library/Arduino15/packages/esp8266/hardware/esp8266/<version>/platform.txt
* Search for "compiler.cpp.flags="
* Add "-fexceptions" at the end

### Windows

* Install arduino_cli to C:\Program Files\Arduino CLI
* Install Arduino IDE
* Install board esp8266 via Arduino IDE (ask google for tutorial)
* Adapt user paths in .vscode/c_cpp_properties.json to your Arduino IDE path

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
