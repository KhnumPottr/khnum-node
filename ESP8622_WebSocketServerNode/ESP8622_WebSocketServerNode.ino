#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <LinkedList.h>

using namespace websockets;

const String sensorName = "esp8622_red";
const char *ssid = "*****";
const char *password = "*****";
const char* websockets_server = "ws://has.local:8080/nodeData";

const int SIZE = 10;
LinkedList<int> moistureList = LinkedList<int>();

const int DRY = 610;
const int WET = 305;

const int inputPin1 = A0;

int oldAvg = 0;
int lastReportedAvg = 0;
unsigned long lastMillis;

bool wsConnected = false;
WebsocketsClient client;

void setup() {
  Serial.begin(115200); // Serial connection
  WiFi.hostname(sensorName);
  WiFi.begin(ssid, password); // WiFi connection

  // Wait for the WiFI connection completion
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  client.connect(websockets_server);
}

void loop() {
  if (client.available()) {
    int moistureReading = analogRead(inputPin1);
    int avg = calculateAverage(moistureReading);
    int avgChange = oldAvg - avg;
    int lastReportedavgChange = lastReportedAvg - avg;
    oldAvg = avg;
    if(avgChange >= 3 || avgChange <= -3 || lastReportedavgChange >= 3 || lastReportedavgChange <= -3){
      lastReportedAvg = avg;
      lastMillis = millis();
      client.send(prepareData(avg));
    } else if(millis() - lastMillis >= 60*60*1000UL){
      lastMillis = millis();
      client.send(prepareData(avg));
    }
    delay(1000);
  }else {
    delay(10000);
    Serial.println("Connecting to controler");
    client.connect(websockets_server);
    if (wsConnected) {
      Serial.println("Connected");
    }
  }
}

String prepareData(int avg) {
  if(avg < 0){
    avg = 0;
  }
  Serial.println("Sending ...");
  String dataToReturn = "{\"nodeName\" : \"" + sensorName + "\", \"messageType\" : \"DATA\", \"payload\" : " + avg + "}";
  return dataToReturn;
}

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Got Message: ");
  Serial.println(message.data());
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    wsConnected = true;
    client.send("{\"nodeName\" : \"" + sensorName + "\", \"messageType\" : \"NEW_NODE\", \"payload\" : \"" + sensorName + "\"}");
    Serial.println("Connection Opened");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    wsConnected = false;
    Serial.println("Connection Closed");
  } else if (event == WebsocketsEvent::GotPing) {
    Serial.println("Got a Ping!");
  } else if (event == WebsocketsEvent::GotPong) {
    Serial.println("Got a Pong!");
  }
}

int calculateAverage(int currentReading) {
  Serial.printf("Current Reading: %d \n", currentReading);
  int sumReadings = 0;
  int rollingAvg = -1;
  if (moistureList.size() > SIZE) {
    moistureList.shift();
  }
  moistureList.add(currentReading);
  if (moistureList.size() >= SIZE) {
    for (int i = 0; i < moistureList.size(); i++) {
      sumReadings += moistureList.get(i);
    }
    rollingAvg = sumReadings / moistureList.size();
  }
  if (rollingAvg == -1) {
    return map(DRY, DRY, WET, 0, 100);
  }
  Serial.printf("rolling average: %d\n", rollingAvg);
  Serial.printf("Percentage: %d\n", map(rollingAvg, DRY, WET, 0, 100));
  return map(rollingAvg, DRY, WET, 0, 100);
}
