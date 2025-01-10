#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
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
int previousTandem = 0;
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 20000; // 20 seconds for testing

const char* authUsername = "skydive";
const char* authPassword = "jump";

// Calibration state
bool isCalibrating = true;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Configure Hall sensor pin
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);

  // Load URL from EEPROM
  loadUrlFromEEPROM();

  WiFiManager wifiManager;
  wifiManager.autoConnect("SkyWin Stats");
  Serial.println("WiFi connected!");

  // Initialize mDNS
  if (MDNS.begin("skywinstats")) {
    Serial.println("mDNS responder started. Access your ESP at http://skywinstats.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("Error starting mDNS responder!");
  }

  // Set up web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set-url", HTTP_POST, handleSetUrl);
  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("Web server started.");

  // Configure stepper motor
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  // Start calibration process
  calibrateToZero();
}

void loop() {
  server.handleClient();
  MDNS.update();

  // Check if it's time to fetch the data
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchData();
    lastFetchTime = millis();
  }

  // Run stepper during calibration
  if (isCalibrating) {
    stepper.run();
  } else {
    if (stepper.distanceToGo() == 0) {
      stepper.disableOutputs();
    }
  }
}

void calibrateToZero() {
  Serial.println("Calibrating to zero position...");
  delay(1000);
  stepper.setMaxSpeed(500);
  stepper.setSpeed(-500);
  stepper.moveTo(-1000000);

  while (true) {
    stepper.run();
    if (millis() % 500 == 0) Serial.println("Searching for Hall sensor...");
    if (digitalRead(HALL_SENSOR_PIN) == LOW) {
      stepper.stop();
      stepper.setCurrentPosition(0);
      Serial.println("Hall sensor detected! Setting current position to zero.");
      int stepsFor110Degrees = -2048 * 195 / 360;
      stepper.moveTo(stepsFor110Degrees);
      stepper.runToPosition();
      stepper.setCurrentPosition(0);
      isCalibrating = false;
      break;
    }
  }
}

void fetchData() {
  if (WiFi.status() == WL_CONNECTED && jsonUrl.length() > 0) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, jsonUrl);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Data fetched: " + payload);

      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.println("Failed to parse JSON!");
        return;
      }

      int tandem = doc["result"][0]["Tandem"];
      Serial.println("Tandem: " + String(tandem));

      // Adjust movement logic for new scale (0-12000)
      int totalSteps = map(constrain(tandem, 0, 12000), 0, 12000, 0, 4096);
      int stepsToMove = totalSteps - stepper.currentPosition();

      stepper.enableOutputs();
      stepper.moveTo(totalSteps);
      stepper.runToPosition();
      previousTandem = tandem;
      stepper.disableOutputs();
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

  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>SkyWin Stats</title>
    </head>
    <body>
      <h1>SkyWin Stats</h1>
      <form action='/set-url' method='POST'>
        <label for='url'>JSON URL:</label>
        <input type='text' id='url' name='url' value=')rawliteral" + jsonUrl + R"rawliteral('>
        <input type='submit' value='Save'>
      </form>
      <a href='/update'>Firmware Update (OTA)</a>
    </body>
    </html>
  )rawliteral";

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
  EEPROM.write(url.length(), '\0');
  EEPROM.commit();
  Serial.println("URL saved to EEPROM: " + url);
}
