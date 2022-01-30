#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <LinkedList.h>


using namespace websockets;

const String sensorName = "***";
const char *ssid = "***";
const char *password = "***";

const int SIZE = 15;
LinkedList<int> moistureList = LinkedList<int>();

const int DRY = 700;
const int WET = 340;

const int ledPin = 2;

int inputPin1 = A0;


int clientsConnected = 0;
const int maxClients = 2;

WebsocketsClient clients[maxClients];
WebsocketsServer server;

void setup() {
  digitalWrite(ledPin, LOW);
  Serial.begin(115200); // Serial connection
  WiFi.hostname(sensorName);
  WiFi.begin(ssid, password); // WiFi connection

  // Wait for the WiFI connection completion
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  
  pinMode(ledPin, 1);
  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  server.listen(81);
}

void loop() {

  if (server.available()) {
    // if there is a client that wants to connect
    if (server.poll() && clientsConnected < maxClients) {
      // accept the connection and register callback
      Serial.print("Accepting a new client, number of client: ");
      Serial.println(clientsConnected + 1);
      // accept client and register onMessage event
      WebsocketsClient client = server.accept();
//      client.onMessage(onMessage);
      
      // store it for later use
      clients[clientsConnected] = client;
      clientsConnected++;
    }
    if (server.poll() && clientsConnected == maxClients) {
      WebsocketsClient client = server.accept();
      client.send("{\"node\" : \"" + sensorName + "\", \"error\" : \"client Capacity full\"}");
      client.close();
    }
    if (clientsConnected > 0) {
      for (int ci = 0; ci <= clientsConnected; ci++) {
        clients[ci].send(prepareData());
      }
    }
    checkConnectedClients();
    delay(1000);
  }
}

String prepareData() {
  int moistureReading = analogRead(inputPin1);
  int avg = calculateAverage(moistureReading);
  String dataToReturn = "{\"node\" : \"" + sensorName +
                        "\", \"moisture\" : " +
                        avg + "}";
  return dataToReturn;
}

void checkConnectedClients()
{
  for (int i = 0; i < clientsConnected; i++)
  {
    if(clients[i].available() == 0 && clientsConnected > 0){
      clients[i].close();
      clientsConnected--;
      Serial.print("Client inactive, disconnecting, number of client: ");
      Serial.println(clientsConnected);
    }
  }
}

int calculateAverage(int currentReading) {
  int sumReadings = 0;
  int rollingAvg = -1;
  if(moistureList.size() > SIZE){
    moistureList.pop();
  }
  moistureList.unshift(currentReading);
  if(moistureList.size() >= SIZE){
    for (int i = 0; i < moistureList.size(); i++){
      sumReadings += moistureList.get(i);
    }
    rollingAvg = sumReadings / SIZE;
  }
  if(rollingAvg == -1){
    return map(DRY, DRY, WET, 0, 100);
  }
  return map(rollingAvg, DRY, WET, 0, 100);
}
