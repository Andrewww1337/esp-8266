

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>

const char* ssid = "***";
const char* password = "***";

const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";

bool ledState = 0;
const int ledPin = 2;


AsyncWebServer server(80);


AsyncWebSocket ws("/ws");


JSONVar readings;


unsigned long lastTime = 0;  
unsigned long timerDelay = 100;



Adafruit_BMP085 bmp;


void initBMP(){
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BME180 sensor, check wiring!");
    while (1);
  }
}


String getSensorReadings(){
  readings["temperature"] = String(bmp.readTemperature());
  readings["humidity"] = String(millis());
  readings["pressure"] = String(bmp.readPressure() * 0.00750063755419211);
  readings["led_status"] = String(ledState);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}


void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else{
   Serial.println("LittleFS mounted successfully");
  }
}


void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
      String sensorReadings = getSensorReadings();
      Serial.print(sensorReadings);
      notifyClients(sensorReadings);
      
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  initBMP();
  initWiFi();
  initFS();
  initWebSocket();



server.on("/fanswich", HTTP_GET, [] (AsyncWebServerRequest *request) {
     ledState = !ledState;
  request->send(200, "text/plain", "OK");
});
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");


  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    String sensorReadings = getSensorReadings();
    Serial.print(sensorReadings);
    notifyClients(sensorReadings);

  lastTime = millis();

  }

  ws.cleanupClients();
}
