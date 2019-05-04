
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
#define MQTT_SERVER "MQTTSERVERIP"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP8266_Neato"
#define MQTT_USER "MQTTUSERNAME"
#define MQTT_PASSWORD "MQTTPASSWORD"
#define MQTT_TOPIC_SUBSCRIBE  "neato/command" //receive
#define MQTT_WILL_TOPIC "neato/state"//publish
//USER CONFIGURED SECTION END//

// Timer
unsigned long intervalcharging = 900000; //Publish Interval while charging
unsigned long interval = 30000; //Publish Interval while cleaning
unsigned long previousMillis = 0;

bool charging=false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
SimpleTimer timer;

int bufferLocation = 0;
uint8_t serialBuffer[8193];

// Variables
bool toggle = true;
const int noSleepPin = 2;
bool boot = true;
long battery_Current_mAh = 0;
long battery_Voltage = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
char battery_percent_send[50];
char battery_Current_mAh_send[50];
uint8_t tempBuf[10];

//Functions
void setup_wifi() 
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }
}

void mqttReconnect() {
  unsigned int retries = 0;

  // Loop until we're reconnected
  while (!mqttClient.connected() && retries < 5) {
    // Attempt to connect, remove user / pass if not needed
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, MQTT_WILL_TOPIC, 0, true, "0")) {
      mqttClient.subscribe(MQTT_TOPIC_SUBSCRIBE);
      mqttClient.publish(MQTT_WILL_TOPIC, "2", true);
      mqttClient.publish("neato/available", "online");
    } else {
      // Wait 5 seconds before retrying
      retries++;
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  
  if (newTopic == "neato/command") 
  {
    if (newPayload == "start_pause" || newPayload == "turn_on")
    {
      Serial.printf("%s\n", "Clean");
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
  sendInfoNeato((char*)"neato/state");
  }
}

const byte numChars = 100;
char receivedChars[numChars]; // an array to store the received data
boolean newData = false;

void sendInfoNeato(char* topic) {

  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  //JSON VARIABLE WITH ALL THE DATA TO PUBLISH
  
  Serial.flush();
  Serial.printf("%s\n", "GetCharger");
  Serial.flush();
  delay(200);
          
  while (Serial.available() > 0 && newData == false) {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127) {
      rc = '_';
    }
    
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
         
      char *auxjson=strstr((char*)receivedChars, (char*)"lPercent,");
      if(auxjson != NULL)
      {
        auxjson=auxjson+9;
        String aux=auxjson;
        String Print=(String)"Battery Level:" + aux;
        mqttClient.publish("neato/state", Print.c_str());   
        aux.replace('\r','\0');
        root["battery_level"] = aux;          
      }

      ndx = 0;
   
    }
  }
  

  long ExternalVoltage=0;
  long VacuumCurrent=0; 
  
  Serial.flush();
  Serial.printf("%s\n", "GetAnalogSensors");
  Serial.flush();
  delay(200);
       
  while (Serial.available() > 0 && newData == false) {
    Serial.flush();
    rc = Serial.read();
    if (rc > 127) {
      rc = '_';
    }
    
    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedChars[ndx] = '\0'; // terminate the string
         
      char *auxjson=strstr((char*)receivedChars, (char*)"ExternalVoltage,mV,");
      if(auxjson != NULL)
      {
        auxjson=auxjson+19;
        String aux=auxjson;
        String Print=(String)"External Voltage:" + aux.toInt();
        mqttClient.publish("neato/state", Print.c_str());   
        aux.replace('\r','\0');
        ExternalVoltage=aux.toInt();        
      }

      auxjson=strstr((char*)receivedChars, (char*)"VacuumCurrent,mA,");
      if(auxjson != NULL)
      {
        auxjson=auxjson+17;
        String aux=auxjson;
        String Print=(String)"Vacuum Current:" + aux.toInt();
        mqttClient.publish("neato/state", Print.c_str());   
        aux.replace('\r','\0');
        VacuumCurrent=aux.toInt();        
      }

      ndx = 0;
   
    }
      if (ExternalVoltage>5000)
      {
        root["charging"] = true;
        root["docked"] = true;
        root["cleaning"] = false;
        charging=true;
      }
      else if(VacuumCurrent>0)
      {
        root["charging"] = false;
        root["docked"] = false;
        root["cleaning"] = true;
        charging=false;
      }
      else
      {
        root["charging"] = false;
        root["docked"] = false;
        root["cleaning"] = false;
        charging=false;
      }
    
  }

  
  String aux=(char*)receivedChars;
  
  String jsonStr;
  root.printTo(jsonStr);
  mqttClient.publish("neato/state", jsonStr.c_str());
  
  //reset the buffer
 receivedChars[0]=' ';
  
  
}

void setup() 
{
  Serial.begin(115200);
  setup_wifi();
  
  ArduinoOTA.setHostname("neato");
  ArduinoOTA.setPassword("neato");
  ArduinoOTA.begin();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() 
{
  ArduinoOTA.handle();
  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
  }

    if(charging==true)
    {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > intervalcharging) {
      mqttClient.publish("neato/available", "online");
      sendInfoNeato((char*)"neato/state");
      previousMillis = currentMillis;
    }
    }
    else
    {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
      mqttClient.publish("neato/available", "online");
      sendInfoNeato((char*)"neato/state");
      previousMillis = currentMillis;
    }
    }
}
