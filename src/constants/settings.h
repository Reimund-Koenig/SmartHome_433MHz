
// Change this names to your need
#define NAME "SmartHomeRF"
#define FILENAME_VERSION "SmartHomeRF_v2.0.1.bin"
#define HTTP_VERSION "rf_current_version.info"
#define HTTP_SERVER_URL "http://reimund-koenig.de/data/smarthome/"
#define CURRENT_VERSION_FS "rf_current_version.txt" // do not change

// Used Pins
#define REICEIVER_DATA 2     // D4
#define REICEIVER_SLEEP 0    // D3
#define RECEIVER_POWER 4     // D2
#define TRANSMITTER_DATA 16  // D0
#define TRANSMITTER_POWER 14 // D5
#define TRANSMITTER_GND 12   // D6
#define EMPTY_01 5           // D1
#define EMPTY_02 4           // D2
#define EMPTY_03 2           // D3

// MQTT Server configuration
#define MQTT_SERVER_IP_ADDRESS "192.168.178.153"
#define MQTT_SERVER_PORT 1883
#define DEFAULT_MQTT_CHANNEL String("home/433")
