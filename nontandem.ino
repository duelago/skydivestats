#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>

// WiFi credentials
const char* ssid = "Alter_3G";
const char* password = "fanta8tter";

// URL to fetch data
// const char* url = "https://demo.skywin.se/api/v1/reports/jump-stats.json";
//TEST URL
const char* url = "http://www.hoppaiplurret.se/jump.php";

// Stepper motor pins
#define IN1 D1
#define IN2 D2
#define IN3 D3
#define IN4 D4

// Hall sensor pin
#define HALL_SENSOR_PIN D5

// Stepper motor setup (using AccelStepper library)
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// Variables
int previousNonTandem = 0;
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 3600000; // 1 hour in milliseconds

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Configure stepper motor
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  // Calibrate to zero position after power failure
  calibrateToZero();
}

void loop() {
  // Check if it's time to fetch the data
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchData();
    lastFetchTime = millis();
  }
}

void calibrateToZero() {
  Serial.println("Calibrating to zero position...");

  // Move the stepper backward slowly until the hall sensor is triggered
  stepper.setSpeed(-500);
  while (digitalRead(HALL_SENSOR_PIN) == HIGH) {
    stepper.runSpeed();
  }

  // Stop the stepper and set the current position as zero
  stepper.setCurrentPosition(0);
  Serial.println("Calibration complete.");
}

void fetchData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Data fetched: " + payload);

      // Parse the JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      int nonTandem = doc["result"][0]["NonTandem"];
      Serial.println("NonTandem: " + String(nonTandem));

      // Move the stepper based on the difference in NonTandem values
      int stepsToMove = (nonTandem - previousNonTandem) * (4000 / 4000);
      stepper.moveTo(stepper.currentPosition() + stepsToMove);
      stepper.runToPosition();

      // Update the previous value
      previousNonTandem = nonTandem;
    } else {
      Serial.println("Error fetching data: " + String(httpCode));
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}
