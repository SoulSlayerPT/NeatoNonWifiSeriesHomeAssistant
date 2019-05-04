# Neato Non Wifi Series Connect to Home Assistant
ESP8266 Wifi to Connect Neato Non Wifi Series to Home Assistant

## Requirements:

### What you need:
- ESP8266 NodeMCU V3
- Neato Vaccum
- Home assistant
- FTDI

### Setup Hardware:

I used dupont wires with connectors and soldered them to Neato serial port then connected to NodeMCU pins. Place the MCU with tape in any place you can fit it inside the neato

Neato will supply the NodeMCU V3 Module

* TX Neato -> RX NodeMCU
* RX Neato -> TX NodeMCU
* 3V3 Neato -> 3V3 NodeMCU
* GND Neato -> GND NodeMCU

### Setup Software:

ESP8266 NodeMCU side:

Use arduino IDE with NeatoNonWifiHA.ino and edit your wifi SSID and password:

const char* ssid = "YOURWIFISSID";
const char* password = "YOURWIFIPASSWORD";

Edit your MQTT Server, username and password:
#define MQTT_SERVER "MQTTSERVERIP"
#define MQTT_USER "MQTTUSERNAME"
#define MQTT_PASSWORD "MQTTPASSWORD"

Upload the file to the neato then you can upload future versions by OTA

Can also debug the MQTT messages for example with chrome app MQTTLens

### Setup Home assistant:

```yaml
vacuum:
  - platform: mqtt
    name: "YOURNEATO"
    supported_features:
      - turn_on
      - turn_off
      - pause
      - stop
      - battery
      - status
      - locate
      - clean_spot
      - send_command
    command_topic: "neato/command"
    battery_level_topic: "neato/state"
    battery_level_template: "{{ value_json.battery_level }}"
    charging_topic: "neato/state"
    charging_template: "{{ value_json.charging }}"
    cleaning_topic: "neato/state"
    cleaning_template: "{{ value_json.cleaning }}"
    docked_topic: "neato/state"
    docked_template: "{{ value_json.docked }}"
    send_command_topic: 'neato/send_command'
    availability_topic: "neato/available"
    payload_available: "online"
    payload_not_available: "offline"
```
