#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>

using namespace websockets;

const String sensorName = "esp8622_red";
const char *ssid = "*****";
const char *password = "*****";
const char* websockets_server = "ws://has.local:8080/irrigate";


const int relay = 5;

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
  pinMode(relay, OUTPUT);
}

void loop() {
  if (!client.available()) {
    delay(5000);
    Serial.println("Attempting to connect to controler");
    client.connect(websockets_server);
    if (wsConnected) {
      Serial.println("Connected");
      client.onMessage(onMessageCallback);
    }
  }
  client.poll();
  delay(500);
}

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Got Message: ");
  Serial.println(message.data());
  const boolean powerOn = message.data() == "true";
  if(powerOn){
    digitalWrite(relay, HIGH);
  } else {
    digitalWrite(relay, LOW);
  }
  
}

void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    wsConnected = true;
    Serial.println("Connection Opened");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    wsConnected = false;
    Serial.println("Connection Closed");
  }
}

void onSwitch() {
  digitalWrite(relay, LOW);
}
