# Neato Non Wifi Series Connect to Home Assistant
ESP8266 Wifi to Connect Neato Non Wifi Series to Home Assistant

With Home Assistant you can also setup schedules with automations

## Requirements:

### What you need:
- ESP8266 NodeMCU V3
- Neato Vaccum
- Home Assistant setup
- FTDI

### Setup Hardware:

I used dupont wires with connectors and soldered them to Neato serial port then connected to NodeMCU pins. Place the MCU with tape in any place you can fit it inside the Neato

Neato will supply the NodeMCU V3 Module

* TX Neato -> RX NodeMCU
* RX Neato -> TX NodeMCU
* 3V3 Neato -> 3V3 NodeMCU
* GND Neato -> GND NodeMCU

![Neato Serial Connector](https://github.com/SoulSlayerPT/NeatoNonWifiSeriesHomeAssistant/raw/master/images/pins.jpg)

### Setup Software:

ESP8266 NodeMCU side:

Use arduino IDE with NeatoNonWifiHA.ino and edit your wifi SSID and password:

* const char* ssid = "YOURWIFISSID";
* const char* password = "YOURWIFIPASSWORD";

Edit your MQTT Server, username and password:
* #define MQTT_SERVER "MQTTSERVERIP"
* #define MQTT_USER "MQTTUSERNAME"
* #define MQTT_PASSWORD "MQTTPASSWORD"

Upload the file to the neato then you can upload future versions by OTA

Can also debug the MQTT messages for example with chrome app MQTTLens

### Setup Home assistant:

```yaml
vacuum:
  - platform: mqtt
    unique_id: unique_id__neato_botvac_wifi
    schema: state
    name: Neato Wifi
    supported_features:
      - start
      - pause
      - stop
      - battery
      - status
      - locate
      - clean_spot
      - send_command
    command_topic: "neato/command"
    state_topic: "neato/state"
    send_command_topic: "neato/send_command"
    availability_topic: "neato/available"
```
