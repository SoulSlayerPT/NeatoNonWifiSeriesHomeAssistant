#include <Arduino.h>

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SimpleTimer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

//USER CONFIGURED SECTION START//
#define MAX_BUFFER 8192
const char* ssid = "YOURWIFISSID";
const char* password = "YOURWIFIPASSWORD";
#define DEVICENAME "neatowifi"
#define MQTT_SERVER "MQTTSERVERIP"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "neatowifi"
#define MQTT_USER "MQTTUSERNAME"
#define MQTT_PASSWORD "MQTTPASSWORD"
#define MQTT_TOPIC_STATE "neato/state"
#define MQTT_TOPIC_COMMAND "neato/command"
#define MQTT_TOPIC_COMMAND_RAW "neato/send_command"
#define MQTT_TOPIC_AVAILABLE "neato/available"
#define MQTT_TOPIC_ERROR "neato/error"
#define MQTT_TOPIC_DEBUG "neato/debug"
//USER CONFIGURED SECTION END//

// Timer
unsigned long intervalcharging = 60*1000; //Publish interval while charging
unsigned long interval = 15*1000; //Publish interval while cleaning
unsigned long previousMillis = 0;

bool charging = false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
SimpleTimer timer;

// int bufferLocation = 0;
// uint8_t serialBuffer[8193];

// Unused variables
// bool toggle = true;
// const int noSleepPin = 2;
// bool boot = true;
// long battery_Current_mAh = 0;
// long battery_Voltage = 0;
// long battery_Total_mAh = 0;
// long battery_percent = 0;
// char battery_percent_send[50];
// char battery_Current_mAh_send[50];
// uint8_t tempBuf[10];

const int numChars = 256;
char receivedChars[numChars]; // an array to store the received data
// boolean newData = false;

//Functions
void setup_wifi()
{
  WiFi.setHostname(DEVICENAME);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

void rebootEvent()
{
  ESP.reset();
}

void mqttReconnect()
{
  unsigned int retries = 0;

  // Loop until we're reconnected
  while (!mqttClient.connected() && retries < 5)
  {
    // Attempt to connect, remove user / pass if not needed
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, MQTT_TOPIC_AVAILABLE, 0, true, "offline"))
    {
      mqttClient.publish(MQTT_TOPIC_COMMAND, "", false);
      mqttClient.publish(MQTT_TOPIC_COMMAND_RAW, "", false);
      mqttClient.publish(MQTT_TOPIC_AVAILABLE, "online", true);
      mqttClient.publish(MQTT_TOPIC_STATE, "", false);
      mqttClient.publish(MQTT_TOPIC_DEBUG, "", false);
      mqttClient.subscribe(MQTT_TOPIC_COMMAND);
      mqttClient.subscribe(MQTT_TOPIC_COMMAND_RAW);
    }
    else
    {
      // Wait 5 seconds before retrying
      retries++;
      delay(5000);
    }
  }
}

void sendInfoNeato()
{
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  StaticJsonDocument<200> root;

  //JSON VARIABLE WITH ALL THE DATA TO PUBLISH

  Serial.flush();
  Serial.printf("%s\n", "GetCharger");
  Serial.flush();
  delay(200);

  while (Serial.available() > 0)
  {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127)
    {
      rc = '_';
    }

    if (rc != endMarker)
    {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars)
      {
        ndx = numChars - 1;
      }
    }
    else
    {
      receivedChars[ndx] = '\0'; // terminate the string
      char *auxjson = strstr((char *)receivedChars, (char *)"lPercent,");
      if (auxjson != NULL)
      {
        auxjson = auxjson + 9;
        String aux = auxjson;
        aux.replace('\r', '\0');
        if (aux == "" || aux == "-FAIL-")
        {
          root["battery_level"] = aux;
        }
        else
        {
          root["battery_level"] = aux.toInt();
        }
      }

      ndx = 0;
    }
  }

  long ExternalVoltage = 0;
  long VacuumCurrent = 0;

  Serial.flush();
  Serial.printf("%s\n", "GetAnalogSensors");
  Serial.flush();
  delay(200);

  while (Serial.available() > 0)
  {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127)
    {
      rc = '_';
    }

    if (rc != endMarker)
    {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars)
      {
        ndx = numChars - 1;
      }
    }
    else
    {
      receivedChars[ndx] = '\0'; // terminate the string
      char *auxjson = strstr((char *)receivedChars, (char *)"ExternalVoltage,mV,");
      if (auxjson != NULL)
      {
        auxjson = auxjson + 19;
        String aux = auxjson;
        aux.replace('\r', '\0');
        ExternalVoltage = aux.toInt();
        root["ExternalVoltage"] = ExternalVoltage;
      }

      auxjson = strstr((char *)receivedChars, (char *)"VacuumCurrent,mA,");
      if (auxjson != NULL)
      {
        auxjson = auxjson + 17;
        String aux = auxjson;
        aux.replace('\r', '\0');
        VacuumCurrent = aux.toInt();
        root["VacuumCurrent"] = VacuumCurrent;
      }

      ndx = 0;
    }
    if (ExternalVoltage > 5000)
    {
      root["charging"] = true;
      root["docked"] = true;
      root["cleaning"] = false;
      root["state"] = "docked";
      charging = true;
    }
    else if (VacuumCurrent > 0)
    {
      root["charging"] = false;
      root["docked"] = false;
      root["cleaning"] = true;
      charging = false;
      root["state"] = "cleaning";
    }
    else
    {
      root["charging"] = false;
      root["docked"] = false;
      root["cleaning"] = false;
      charging = false;
      root["state"] = "idle";
    }
    // TODO state error, returning, paused
  }

  String aux = (char *)receivedChars;

  String jsonStr;
  serializeJsonPretty(root, jsonStr);
  mqttClient.publish("neato/state", jsonStr.c_str());

  //reset the buffer
  receivedChars[0] = ' ';
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);

  if (newTopic == "neato/command")
  // Reference: https://help.neatorobotics.com/wp-content/uploads/2020/07/XV-ProgrammersManual-3_1.pdf
  {
    Serial.flush();
    if (newPayload == "start" || newPayload == "turn_on")
    {
      Serial.printf("%s\n", "Clean House");
    }
    if (newPayload == "pause")
    {
      // Skip menu query on display
      Serial.printf("%s\n", "Clean House");
      Serial.flush();
      delay(500);
      Serial.printf("%s\n", "Clean House");
    }
    if (newPayload == "stop" || newPayload == "turn_off")
    {
      Serial.printf("%s\n", "Clean Stop");
    }
    if (newPayload == "clean_spot")
    {
      Serial.printf("%s\n", "Clean Spot");
    }
    if (newPayload == "locate")
    {
      Serial.printf("%s\n", "PlaySound 3");
    }
    Serial.flush();
  }
  // Send a custom command, like SetTime
  else if (newTopic == "neato/send_command")
  {
    Serial.flush();
    Serial.printf("%s\n", newPayload.c_str());
    Serial.flush();
  }

  // Send returned status message
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  delay(200);
  while (Serial.available() > 0)
  {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127)
    {
      rc = '_';
    }

    if (rc != endMarker)
    {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars)
      {
        ndx = numChars - 1;
      }
    }
    else
    {
      receivedChars[ndx] = '\0'; // terminate the string
      mqttClient.publish(MQTT_TOPIC_DEBUG, (char *)receivedChars);
      ndx = 0;
    }
  }
  //reset the buffer
  receivedChars[0] = ' ';

  delay(200);
  sendInfoNeato();
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();

  ArduinoOTA.setHostname(DEVICENAME);
  ArduinoOTA.setPassword(DEVICENAME);

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("ESP-12x: OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("ESP-12x: OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("ESP-12x: OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("ESP-12x: OTA Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("ESP-12x: OTA End Failed");
  });
  ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();
  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  if (mqttClient.connected())
  {
    mqttClient.loop();
  }

  if (charging == true)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > intervalcharging)
    {
      mqttClient.publish(MQTT_TOPIC_AVAILABLE, "online", true);
      sendInfoNeato();
      previousMillis = currentMillis;
    }
  }
  else
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval)
    {
      mqttClient.publish(MQTT_TOPIC_AVAILABLE, "online", true);
      sendInfoNeato();
      previousMillis = currentMillis;
    }
  }
}
