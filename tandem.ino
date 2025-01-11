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
    // Add web service to mDNS
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

  // Start calibration process in non-blocking way
  calibrateToZero();
}

void loop() {
  server.handleClient();
  MDNS.update(); // Handle mDNS requests
  

  // Check if it's time to fetch the data
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchData();
    lastFetchTime = millis();
  }

  // If still calibrating, let the motor run in background
  if (isCalibrating) {
    stepper.run();
  } else {
    // If the motor is not moving, disable the outputs to avoid unnecessary torque
    if (stepper.distanceToGo() == 0) {
      stepper.disableOutputs();  // Disable motor outputs to stop torque
    }
  }
}


void calibrateToZero() {
  Serial.println("Calibrating to zero position...");
  delay(1000);  // Delay to allow initialization

  // Move motor in reverse direction continuously
  stepper.setMaxSpeed(500);
  stepper.setSpeed(-500);
  stepper.moveTo(-1000000);  // Arbitrary large negative number to keep the motor moving

  while (true) {
    stepper.run();  // Continuously run the motor

    // Print status occasionally to reduce flooding
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 500) {
      Serial.println("Searching for Hall sensor...");
      lastPrintTime = millis();
    }

    // If the Hall sensor is triggered, stop the motor and set current position to zero
    if (digitalRead(HALL_SENSOR_PIN) == LOW) {  // Assuming LOW indicates sensor triggered
      stepper.stop();  // Stop the motor
      stepper.setCurrentPosition(0);  // Set the current position to zero
      Serial.println("Hall sensor detected! Setting current position to zero.");

      // Move stepper to get to 0. Adjust the degrees so the arrow is straight up (Change the value 199)
      int stepsFor110Degrees = -2048 * 199 / 360;  // Calculate steps for 195 degrees. Change accordingly
      stepper.moveTo(stepsFor110Degrees);
      stepper.runToPosition();  // Move and stop

      // Update the new zero point
      stepper.setCurrentPosition(0);
      Serial.println("New zero point set after moving 199 degrees to 0");

      isCalibrating = false;  // Mark calibration as done
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

      // Update the JSON buffer size to fit the new structure
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.println("Failed to parse JSON!");
        return;
      }

      // Access the "result" array and find "Tandem"
      int tandem = doc["result"][0]["Tandem"];
      Serial.println("Tandem: " + String(tandem));

      // Handle values above 4000
      int baseSteps = map(min(tandem, 4000), 0, 4000, 0, 4096);
      int extraSteps = (tandem > 4000) ? tandem - 4000 : 0;
      int totalSteps = baseSteps + extraSteps;

      // Calculate steps to move
      int stepsToMove = totalSteps - stepper.currentPosition();
      stepper.moveTo(totalSteps);

      // Re-enable motor before moving
      stepper.enableOutputs();

      // Run the motor to the new position
      stepper.runToPosition();

      previousTandem = tandem;

      // After movement, disable the motor to avoid torque
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
      <style>
        body {
          font-family: 'Arial', sans-serif;
          background-color: #f0f8ff;
          margin: 0;
          padding: 20px;
        }
        h1 {
          color: #333;
          text-align: center;
        }
        h3 {
          color: #696969;
          text-align: center;
        }
        form {
          max-width: 400px;
          margin: 0 auto;
          background-color: #ffffff;
          padding: 20px;
          border-radius: 10px;
          box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        }
        label {
          display: block;
          margin-bottom: 8px;
          font-weight: bold;
        }
        input[type="text"] {
          width: 100%;
          padding: 10px;
          margin-bottom: 20px;
          border: 1px solid #ccc;
          border-radius: 5px;
        }
        input[type="submit"] {
          width: 100%;
          padding: 10px;
          background-color: #007bff;
          color: #fff;
          border: none;
          border-radius: 5px;
          cursor: pointer;
        }
        input[type="submit"]:hover {
          background-color: #0056b3;
        }
        a {
          display: block;
          text-align: center;
          margin-top: 20px;
          color: #007bff;
          text-decoration: none;
        }
        a:hover {
          color: #0056b3;
        }
        @media (max-width: 600px) {
          body {
            padding: 10px;
          }
          form {
            padding: 15px;
          }
        }
      </style>
    </head>
    <body>
      <h1>SkyWin Stats</h1>
      <form action='/set-url' method='POST'>
        <label for='url'>JSON URL:</label>
        <input type='text' id='url' name='url' value=')rawliteral" + jsonUrl + R"rawliteral('>
        <input type='submit' value='Save'>
      </form>
      <a href='/update'>Firmware Update (OTA)</a>
      <p></p>
      <h3>SkyWinOne 24.0.1 or newer needed for it to work<br>Demo URL to fetch random number of jumps:<br> http://www.hoppaiplurret.se/jump.php </h3><p></p>
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
  EEPROM.write(url.length(), '\0'); // Null-terminate the string
  EEPROM.commit();
  Serial.println("URL saved to EEPROM: " + url);
}
