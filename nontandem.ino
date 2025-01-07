#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ElegantOTA.h>

// Pin definitions
#define IN1 D1
#define IN2 D2
#define IN3 D3
#define IN4 D4
#define HALL_SENSOR_PIN D5

// Variables
ESP8266WebServer server(80);
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);
String jsonUrl;
int previousNonTandem = 0;
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 3600000; // 1 hour in milliseconds

const char* authUsername = "skydive";
const char* authPassword = "jump";

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Load URL from EEPROM
  loadUrlFromEEPROM();

  // WiFi configuration with WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP8266-Stepper");
  Serial.println("WiFi connected!");

  // Set up web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set-url", HTTP_POST, handleSetUrl);
  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("Web server started.");

  // Configure stepper motor
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  // Calibrate to zero position
  calibrateToZero();
}

void loop() {
  server.handleClient();

  // Check if it's time to fetch the data
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchData();
    lastFetchTime = millis();
  }
}

void calibrateToZero() {
  Serial.println("Calibrating to zero position...");
  stepper.setSpeed(-500);
  while (digitalRead(HALL_SENSOR_PIN) == HIGH) {
    stepper.runSpeed();
  }
  stepper.setCurrentPosition(0);
  Serial.println("Calibration complete.");
}

void fetchData() {
  if (WiFi.status() == WL_CONNECTED && jsonUrl.length() > 0) {
    HTTPClient http;
    http.begin(jsonUrl);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Data fetched: " + payload);

      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      int nonTandem = doc["result"][0]["NonTandem"];
      Serial.println("NonTandem: " + String(nonTandem));

      int stepsToMove = (nonTandem - previousNonTandem) * (4000 / 4000);
      stepper.moveTo(stepper.currentPosition() + stepsToMove);
      stepper.runToPosition();

      previousNonTandem = nonTandem;
    } else {
      Serial.println("Error fetching data: " + String(httpCode));
    }

    http.end();
  } else {
    Serial.println("WiFi not connected or URL not set");
  }
}

void handleRoot() {
  if (!server.authenticate(authUsername, authPassword)) {
    return server.requestAuthentication();
  }

  String html = "<html><head><title>ESP8266 Stepper Control</title></head><body>";
  html += "<h1>ESP8266 Stepper Control</h1>";
  html += "<form action='/set-url' method='POST'>";
  html += "<label for='url'>JSON URL:</label><br>";
  html += "<input type='text' id='url' name='url' value='" + jsonUrl + "'><br><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form><br>";
  html += "<a href='/update'>Firmware Update (OTA)</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSetUrl() {
  if (!server.authenticate(authUsername, authPassword)) {
    return server.requestAuthentication();
  }

  if (server.method() == HTTP_POST) {
    jsonUrl = server.arg("url");
    saveUrlToEEPROM(jsonUrl);
    server.send(200, "text/html", "<html><body><h1>URL Saved!</h1><a href='/'>Back to Home</a></body></html>");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void loadUrlFromEEPROM() {
  char urlBuffer[100];
  for (int i = 0; i < sizeof(urlBuffer); i++) {
    urlBuffer[i] = EEPROM.read(i);
  }
  jsonUrl = String(urlBuffer);
  jsonUrl.trim();
  Serial.println("Loaded URL: " + jsonUrl);
}

void saveUrlToEEPROM(String url) {
  for (int i = 0; i < url.length() && i < 100; i++) {
    EEPROM.write(i, url[i]);
  }
  EEPROM.write(url.length(), '\0'); // Null-terminate the string
  EEPROM.commit();
  Serial.println("URL saved to EEPROM: " + url);
}
