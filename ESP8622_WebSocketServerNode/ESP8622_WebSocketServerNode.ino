#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <LinkedList.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoJson.h>

using namespace websockets;

Adafruit_ADS1015 ads;

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// Sensor Config
const String sensorName = "esp8622_blue";
const char *ssid = "OrbiWifiMesh";
const char *password = "97e977fb";
//const char* websockets_server = "ws://has.local:8080/nodeData";
const char* websockets_server = "ws://192.168.1.14:8080/nodeData";

//JSON deserialize 
DynamicJsonDocument doc(1024);

// Irrigation data;
LinkedList<int> moistureList = LinkedList<int>();
const int DRY = 610;
const int WET = 305;
bool isIrrigating = false;
int MAX_IRRIGATE = 0;

// Function Flags
bool READING = true;
bool EXECUTING = true;
unsigned long lastMillis;

// Websocket data
bool wsConnected = false;
WebsocketsClient client;

/**
 * On Boot this code will execute once
 */
void setup() {
  Serial.begin(115200); // Serial connection
  WiFi.hostname(sensorName);
  WiFi.begin(ssid, password); // WiFi connection
  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);

  // Wait for the WiFI connection completion
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }

  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  client.connect(websockets_server);
  
  lastMillis = millis();

  requestCommand();
}

/**
 * This code will constantly loup
 */
void loop() {
  if (client.available()) {
    /**
     * Planter Wake
     * 1. client.send(getMoistureReading()); DONE
     * 2. read commands 
     * 3. if any outstanding commands -> execute
     * 4. report executed commands
     * 5. sleep
     */
    if(READING){
      int avg = getMoistureReading();
      if(avg != -1){
        client.send(prepareData(avg));
      } else {
        lastMillis = millis();
      }
    }
    if(!READING){
      client.poll();
    }
    if(isIrrigating){
      int avg = getMoistureReading();
      if(avg >= MAX_IRRIGATE){
        stopIrrigation();
      }
      lastMillis = millis();
    }
    if(millis() - lastMillis >= 15e3){
      client.send("{\"id\" : \"" + sensorName + "\", \"messageType\" : \"SLEEPING\", \"payload\" : null}");
      Serial.println("I'm awake, but I'm going into deep sleep mode for 30 seconds");
      ESP.deepSleep(30e6); 
    }
    delay(2000);
  } else {
    delay(10000);
    Serial.println("Connecting to controler");
    client.connect(websockets_server);
    if (wsConnected) {
      Serial.println("Connected");
    }
  }
}

/**
 * ### SERVICE COMMUNICATION ###
 */

 String prepareData(int avg) {
  if(avg < 0){
    avg = 0;
  }
  Serial.println("Sending ...");
  String dataToReturn = "{\"id\" : \"" + sensorName + "\", \"messageType\" : \"NODE_DATA_REPORT\", \"payload\" : " + avg + "}";
  return dataToReturn;
}


/**
 * ### WEBSOCKET Functions ###
 */

struct commandMessage {
  char id[6];
  char commandMessage[13];
  char payload[5];
};
void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Got Message: ");
  Serial.println(message.data());
  String test =
      "{\"commandType\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
  Serial.println(test);
  auto error = deserializeJson(doc, message.data());
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }
  JsonObject obj = doc.as<JsonObject>();
  Serial.println(obj);
  int commandType = obj["commandType"];
  Serial.println(commandType);
  switch(commandType) {
    case 1910:
      Serial.println("IRRIGATE");
      startIrrigation();
      break;
    case 1911:
      Serial.println("IRRIGATE MAX VAL");
      MAX_IRRIGATE = doc["payload"];
      break;
    case 2001:

      break;
    case 2002:

      break;
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    wsConnected = true;
    client.send("{\"id\" : \"" + sensorName + "\", \"messageType\" : \"NODE_NEW_CONNECTION\", \"payload\" : \"" + sensorName + "\"}");
    Serial.println("Connection Opened");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    wsConnected = false;
    Serial.println("Connection Closed");
  }
}

void requestCommand() {
  String request = "{\"id\" : \"" + sensorName + "\", \"messageType\" : \"COMMAND_REQUEST\", \"payload\" : null}";
  client.send(request);
}

/**
 * ### IRRIGATION ###
 */

void startIrrigation(){
  isIrrigating = true;
  client.send("{\"id\" : \"" + sensorName + "\", \"commandType\" : \"IRRIGATING\", \"payload\" : null}");
  digitalWrite(14, HIGH);
}

void stopIrrigation(){
  isIrrigating = false;
  digitalWrite(14, LOW);
}

void checkIrrigtaion(int avg){
  if(avg >= MAX_IRRIGATE){
    stopIrrigation();
    client.send("{\"id\" : \"" + sensorName + "\", \"commandType\" : \"STOPPED_IRRIGATING\", \"payload\" : null}");
  }
}

/**
 * ### MOISTURE Reading ###
 */
 
 /**
  * >>> getMoistureReading()
  * Gets the moisture reading from all 4 sesnors
  * feeds them to the calculateAverage function and
  * returns the percentage
  */
int getMoistureReading() {
  int16_t adc0, adc1, adc2, adc3;
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);

 int meanFour = (adc0 + adc1 + adc2 + adc3) / 4;
 
 return calculateAverage(meanFour);
}

/**
 * >>> calculateAverage(int currentReading)
 * Calculates the rolling average in percentage form
 * 
 * DRY -> This global vvar states the value of 0% moisture
 *
 * WET -> This global var states the value of 100% moisture 
 */
int calculateAverage(int currentReading) {
  Serial.printf("Current Reading: %d \n", currentReading);
  int sumReadings = 0;
  int rollingAvg = -1;
  if (moistureList.size() > 5) {
    moistureList.shift();
  }
  moistureList.add(currentReading);
  if (moistureList.size() >= 5) {
    for (int i = 0; i < moistureList.size(); i++) {
      sumReadings += moistureList.get(i);
    }
    rollingAvg = sumReadings / moistureList.size();
  }
  if (rollingAvg == -1) {
    return -1;
  }
  READING = false;
  Serial.printf("rolling average: %d\n", rollingAvg);
  Serial.printf("Percentage: %d\n", map(rollingAvg, DRY, WET, 0, 100));
  return map(rollingAvg, DRY, WET, 0, 100);
}
