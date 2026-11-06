/*
  Lavinmätare – Saalbach-Hinterglemm
  ===================================
  Wemos D1 mini (ESP8266) + 28BYJ-48 / ULN2003 + hallsensor (LM393-modul)
  Visar aktuell lavinfara för "Kitzbüheler Alpen Glemmtal" (AT-05-14)
  på en höjdmätar-urtavla:  1 -> "1",  2 -> "2",  3 -> "3",  4 & 5 -> "0" (toppen)

  Datakälla: Lawinenwarndienst Salzburg (ALBINA-systemet), CAAMLv6 JSON:
    https://static.lawinen-warnung.eu/bulletins/YYYY-MM-DD/YYYY-MM-DD_AT-05_de_CAAMLv6.json
  Mappens datum = bulletinens giltighetsdag. Bulletinen publiceras ca 17:00
  dagen innan och gäller till 17:00. Vi provar därför "imorgon" först, sedan "idag".
  Finns ingen fil (404) är det utanför säsong.

  Bibliotek (Library Manager):
    - WiFiManager   (tzapu)
    - ElegantOTA    (ayushsharma82) v3.x   – standardläge (synkron ESP8266WebServer)
    - AccelStepper  (Mike McCauley)
    - ArduinoJson   (Benoit Blanchon) v7.x
  Board: "LOLIN(WEMOS) D1 R2 & mini", Flash Size: 4MB (FS:1MB OTA:~1019KB)

  Webbgränssnitt: http://lavinmatare.local/   OTA: http://lavinmatare.local/update
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>
#include <ElegantOTA.h>
#include <AccelStepper.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include <math.h>

#define FW_VERSION "1.0.0"
#define HOSTNAME   "lavinmatare"

// ---------- Pinnar (enligt kopplingsschemat) ----------
#define PIN_IN1  D1
#define PIN_IN2  D2
#define PIN_IN3  D3
#define PIN_IN4  D4
#define HALL_PIN D5          // DO från hallsensorn. Ändra till D0 om den sitter där.

// ---------- Konstanter ----------
const char*    TZ_INFO           = "CET-1CEST,M3.5.0,M10.5.0/3";
const char*    DATA_HOST         = "static.lawinen-warnung.eu";
const uint32_t FETCH_INTERVAL_MS = 3600UL * 1000UL;   // en gång per timme
const uint32_t RETRY_ERROR_MS    = 10UL * 60UL * 1000UL;
const uint32_t MANUAL_TIMEOUT_MS = 10UL * 60UL * 1000UL; // test/kalibrering släpper efter 10 min
const int      HALL_DEBOUNCE     = 20;

// ---------- Konfiguration (sparas i LittleFS) ----------
struct Config {
  int   stepsPerRev    = 4096;    // HALF4WIRE. Verkligt värde för 28BYJ-48 är ~4076
  long  zeroOffset     = 0;       // steg från magnetens mitt till "0" på tavlan
  bool  invertDir      = false;   // vänd rotationsriktning (kräver omstart)
  bool  hallActiveLow  = true;    // de flesta LM393-moduler drar DO låg vid magnet
  float offseasonValue = 3.5;     // var visaren parkeras när ingen bulletin finns (0–4)
  int   speed          = 600;     // steg/s
  char  region[16]     = "AT-05-14";
} cfg;

ESP8266WebServer server(80);
AccelStepper* stepper = nullptr;

// ---------- Motortillstånd ----------
enum MotorState { M_IDLE, M_LEAVE_MAGNET, M_SEEK_MAGNET, M_ON_MAGNET, M_MOVING };
MotorState mState = M_IDLE;
bool  homed = false;
bool  moveAfterHome = false;
bool  movingToDial = false;
bool  motorFault = false;
long  homingStartPos = 0;
long  magnetEntry = 0;
long  magnetWidth = 0;
int   hallCnt = 0;
float targetDialValue = 0;
float shownDialValue = -1;      // -1 = okänd position
String motorError = "";

// ---------- Data ----------
int    liveLevel = -1;          // -1 okänd, 0 ingen bulletin, 1–5
String liveRaw = "";
String liveValidUntil = "";
String liveSource = "";
String lastFetchStatus = "Ej hämtat ännu";
String fetchErr = "";
time_t lastFetchTime = 0;
uint32_t lastFetchMs = 0;
bool   fetchRequested = true;

// ---------- Manuellt läge (test/kalibrering) ----------
bool     manual = false;
bool     testActive = false;
int      testLevel = 0;
uint32_t manualMs = 0;

// =====================================================================
//  Hjälpfunktioner
// =====================================================================
long modp(long a, long m) { long r = a % m; return r < 0 ? r + m : r; }

bool hallActive() {
  bool v = digitalRead(HALL_PIN);
  return cfg.hallActiveLow ? !v : v;
}

bool hallSettled(bool want) {
  if (hallActive() == want) { if (hallCnt < 10000) hallCnt++; }
  else hallCnt = 0;
  return hallCnt >= HALL_DEBOUNCE;
}

int levelFromString(const char* s) {
  if (!strcmp(s, "low"))          return 1;
  if (!strcmp(s, "moderate"))     return 2;
  if (!strcmp(s, "considerable")) return 3;
  if (!strcmp(s, "high"))         return 4;
  if (!strcmp(s, "very_high"))    return 5;
  return 0;                       // no_snow, no_rating m.m.
}

// Lavinnivå -> värde på urtavlan (0–4, där 4 = 0 = toppen)
float dialValueForLevel(int lvl) {
  switch (lvl) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4:
    case 5: return 0;
    default: return cfg.offseasonValue;
  }
}

long stepsForDial(float v) {
  return modp(lround(v * cfg.stepsPerRev / 4.0), cfg.stepsPerRev);
}

float desiredDial() {
  if (manual) return testActive ? dialValueForLevel(testLevel) : NAN;
  if (liveLevel < 0) return NAN;
  return dialValueForLevel(liveLevel);
}

// =====================================================================
//  Konfiguration
// =====================================================================
void saveConfig() {
  JsonDocument d;
  d["spr"]      = cfg.stepsPerRev;
  d["zero"]     = cfg.zeroOffset;
  d["invert"]   = cfg.invertDir;
  d["hallLow"]  = cfg.hallActiveLow;
  d["offseason"]= cfg.offseasonValue;
  d["speed"]    = cfg.speed;
  d["region"]   = cfg.region;
  File f = LittleFS.open("/config.json", "w");
  if (f) { serializeJson(d, f); f.close(); }
}

void loadConfig() {
  if (!LittleFS.exists("/config.json")) return;
  File f = LittleFS.open("/config.json", "r");
  if (!f) return;
  JsonDocument d;
  if (!deserializeJson(d, f)) {
    cfg.stepsPerRev    = d["spr"]       | cfg.stepsPerRev;
    cfg.zeroOffset     = d["zero"]      | cfg.zeroOffset;
    cfg.invertDir      = d["invert"]    | cfg.invertDir;
    cfg.hallActiveLow  = d["hallLow"]   | cfg.hallActiveLow;
    cfg.offseasonValue = d["offseason"] | cfg.offseasonValue;
    cfg.speed          = d["speed"]     | cfg.speed;
    strlcpy(cfg.region, d["region"] | cfg.region, sizeof(cfg.region));
  }
  f.close();
}

// =====================================================================
//  Motor
// =====================================================================
void stopMotor() {
  stepper->disableOutputs();   // strömlös mellan uppdateringarna
  mState = M_IDLE;
}

void motorFail(const String& msg) {
  motorError = msg;
  motorFault = true;
  homed = false;
  Serial.println("Motorfel: " + msg);
  stopMotor();
}

// Kör alltid framåt (samma riktning) för att slippa glapp i växellådan
void goToDial(float v) {
  long spr  = cfg.stepsPerRev;
  long base = stepsForDial(v) + cfg.zeroOffset;
  long cur  = stepper->currentPosition();
  long t    = cur + modp(base - cur, spr);
  stepper->setMaxSpeed(cfg.speed);
  stepper->setAcceleration(cfg.speed);
  stepper->enableOutputs();
  stepper->moveTo(t);
  targetDialValue = v;
  movingToDial = true;
  mState = M_MOVING;
}

void startHoming(bool thenMove) {
  motorError = "";
  motorFault = false;
  stepper->setMaxSpeed(cfg.speed);
  stepper->setSpeed(cfg.speed);
  stepper->enableOutputs();
  homingStartPos = stepper->currentPosition();
  moveAfterHome = thenMove;
  hallCnt = 0;
  mState = hallActive() ? M_LEAVE_MAGNET : M_SEEK_MAGNET;
}

void jog(long n) {
  stepper->setMaxSpeed(cfg.speed);
  stepper->setAcceleration(cfg.speed);
  stepper->enableOutputs();
  stepper->move(n);
  movingToDial = false;
  shownDialValue = -1;
  mState = M_MOVING;
}

void motorLoop() {
  switch (mState) {
    case M_IDLE:
      return;

    case M_LEAVE_MAGNET:
    case M_SEEK_MAGNET:
    case M_ON_MAGNET: {
      stepper->runSpeed();
      long pos = stepper->currentPosition();
      if (pos - homingStartPos > 2L * cfg.stepsPerRev) {
        motorFail("Magneten hittades inte på 2 varv – kolla sensor, avstånd och potentiometer");
        return;
      }
      if (mState == M_LEAVE_MAGNET) {
        if (hallSettled(false)) { mState = M_SEEK_MAGNET; hallCnt = 0; }
      } else if (mState == M_SEEK_MAGNET) {
        if (hallSettled(true)) { magnetEntry = pos; mState = M_ON_MAGNET; hallCnt = 0; }
      } else {
        if (hallSettled(false)) {
          long center = (magnetEntry + pos) / 2;
          magnetWidth = pos - magnetEntry;
          stepper->setCurrentPosition(pos - center);   // magnetens mitt = 0
          homed = true;
          if (moveAfterHome) goToDial(targetDialValue);
          else stopMotor();
        }
      }
      break;
    }

    case M_MOVING:
      stepper->run();
      if (stepper->distanceToGo() == 0) {
        if (movingToDial) shownDialValue = targetDialValue;
        stopMotor();
      }
      break;
  }
}

const char* motorStateName() {
  switch (mState) {
    case M_IDLE:         return "vilar (strömlös)";
    case M_LEAVE_MAGNET: return "homing: lämnar magnet";
    case M_SEEK_MAGNET:  return "homing: söker magnet";
    case M_ON_MAGNET:    return "homing: passerar magnet";
    case M_MOVING:       return "kör";
  }
  return "?";
}

// =====================================================================
//  Hämta lavinbulletin
// =====================================================================
const int FETCH_ERROR    = -1;
const int FETCH_NOTFOUND = -2;

int fetchForDate(const char* date, String& raw, String& validUntil) {
  String prov = String(cfg.region).substring(0, 5);   // "AT-05"
  String url  = String("https://") + DATA_HOST + "/bulletins/" + date + "/" +
                date + "_" + prov + "_de_CAAMLv6.json";
  Serial.println("GET " + url);

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  if (client->probeMaxFragmentLength(DATA_HOST, 443, 1024)) client->setBufferSizes(1024, 1024);

  HTTPClient http;
  http.useHTTP10(true);          // inget chunked encoding -> kan streama direkt in i parsern
  http.setTimeout(15000);
  if (!http.begin(*client, url)) { fetchErr = "begin() misslyckades"; return FETCH_ERROR; }

  int code = http.GET();
  if (code == 404) { http.end(); return FETCH_NOTFOUND; }
  if (code != 200) {
    fetchErr = code < 0 ? http.errorToString(code) : ("HTTP " + String(code));
    http.end();
    return FETCH_ERROR;
  }

  // Filtrera bort allt utom det vi behöver (filen är ~16 kB)
  JsonDocument filter;
  filter["bulletins"][0]["regions"][0]["regionID"]       = true;
  filter["bulletins"][0]["dangerRatings"][0]["mainValue"] = true;
  filter["bulletins"][0]["validTime"]["endTime"]          = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { fetchErr = String("JSON: ") + err.c_str(); return FETCH_ERROR; }

  for (JsonObject b : doc["bulletins"].as<JsonArray>()) {
    bool match = false;
    for (JsonObject r : b["regions"].as<JsonArray>()) {
      if (strcmp(r["regionID"] | "", cfg.region) == 0) { match = true; break; }
    }
    if (!match) continue;

    // Högsta nivån i bulletinen (t.ex. "low" under trädgränsen, "considerable" över)
    int maxL = 0;
    raw = "";
    for (JsonObject d : b["dangerRatings"].as<JsonArray>()) {
      const char* mv = d["mainValue"] | "";
      if (raw.length()) raw += ", ";
      raw += mv;
      int l = levelFromString(mv);
      if (l > maxL) maxL = l;
    }
    validUntil = (const char*)(b["validTime"]["endTime"] | "");
    return maxL;
  }
  raw = "";
  validUntil = "";
  return 0;   // regionen fanns inte i bulletinen
}

void doFetch() {
  fetchRequested = false;
  lastFetchMs = millis();
  motorFault = false;           // ge motorn ett nytt försök varje timme

  time_t now = time(nullptr);
  if (now < 1700000000) {
    lastFetchStatus = "Väntar på NTP-tid…";
    lastFetchMs = millis() - FETCH_INTERVAL_MS + 30000UL;
    return;
  }

  char dToday[11], dTomorrow[11];
  struct tm t;
  time_t tm1 = now + 86400;
  localtime_r(&now, &t);  strftime(dToday,    sizeof dToday,    "%Y-%m-%d", &t);
  localtime_r(&tm1, &t);  strftime(dTomorrow, sizeof dTomorrow, "%Y-%m-%d", &t);
  const char* dates[2] = { dTomorrow, dToday };

  for (int i = 0; i < 2; i++) {
    String raw, valid;
    int r = fetchForDate(dates[i], raw, valid);
    if (r == FETCH_NOTFOUND) continue;
    if (r == FETCH_ERROR) {
      lastFetchStatus = "Fel vid hämtning (" + fetchErr + ") – nytt försök om 10 min";
      lastFetchMs = millis() - FETCH_INTERVAL_MS + RETRY_ERROR_MS;
      Serial.println(lastFetchStatus);
      return;   // behåll senast kända nivå
    }
    liveLevel = r;
    liveRaw = raw;
    liveValidUntil = valid;
    liveSource = dates[i];
    lastFetchTime = now;
    lastFetchStatus = r > 0 ? "OK" : "OK – regionen saknas i bulletinen";
    Serial.printf("Nivå %d (%s) från %s\n", r, raw.c_str(), dates[i]);
    return;
  }
  liveLevel = 0;
  liveRaw = "";
  liveValidUntil = "";
  liveSource = "";
  lastFetchTime = now;
  lastFetchStatus = "Ingen bulletin publicerad (utanför säsong)";
  Serial.println(lastFetchStatus);
}

// =====================================================================
//  Webbgränssnitt
// =====================================================================
#include "index_html.h"   // webbsidan (HTML/CSS/JS)

bool busy() {
  if (mState != M_IDLE) { server.send(409, "text/plain", "Motorn är upptagen – vänta tills den står still"); return true; }
  return false;
}

void enterManual(bool test, int lvl) {
  manual = true;
  testActive = test;
  testLevel = lvl;
  manualMs = millis();
}

void handleStatus() {
  JsonDocument d;
  d["fw"]          = FW_VERSION;
  d["ip"]          = WiFi.localIP().toString();
  d["rssi"]        = WiFi.RSSI();
  d["heap"]        = ESP.getFreeHeap();
  d["level"]       = liveLevel;
  d["raw"]         = liveRaw;
  d["validUntil"]  = liveValidUntil;
  d["source"]      = liveSource;
  d["fetchStatus"] = lastFetchStatus;
  if (lastFetchTime) {
    char buf[20]; struct tm t; localtime_r(&lastFetchTime, &t);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &t);
    d["lastFetch"] = buf;
  }
  uint32_t el = millis() - lastFetchMs;
  d["nextFetchMin"] = el >= FETCH_INTERVAL_MS ? 0 : (FETCH_INTERVAL_MS - el) / 60000UL;
  d["manual"]      = manual;
  d["test"]        = testActive;
  d["testLevel"]   = testLevel;
  d["hall"]        = hallActive();
  d["state"]       = motorStateName();
  d["homed"]       = homed;
  d["pos"]         = homed ? modp(stepper->currentPosition(), cfg.stepsPerRev) : 0;
  d["magnetWidth"] = magnetWidth;
  d["zero"]        = cfg.zeroOffset;
  d["shown"]       = shownDialValue;
  d["motorError"]  = motorError;
  d["spr"]         = cfg.stepsPerRev;
  d["speed"]       = cfg.speed;
  d["region"]      = cfg.region;
  d["offseason"]   = cfg.offseasonValue;
  d["invert"]      = cfg.invertDir;
  d["hallLow"]     = cfg.hallActiveLow;
  String out;
  serializeJson(d, out);
  server.send(200, "application/json", out);
}

void setupWeb() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);

  server.on("/api/fetch", HTTP_POST, []() {
    if (busy()) return;
    doFetch();
    server.send(200, "text/plain", lastFetchStatus);
  });

  server.on("/api/test", HTTP_POST, []() {
    if (busy()) return;
    int lvl = server.arg("level").toInt();
    if (lvl < 0 || lvl > 5) { server.send(400, "text/plain", "Ange 0–5"); return; }
    enterManual(true, lvl);
    shownDialValue = -1;            // tvinga homing + förflyttning även om värdet är samma
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/live", HTTP_POST, []() {
    manual = false; testActive = false;
    if (liveLevel < 0) fetchRequested = true;
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/home", HTTP_POST, []() {
    if (busy()) return;
    enterManual(false, 0);
    targetDialValue = 0;
    startHoming(true);
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/jog", HTTP_POST, []() {
    if (busy()) return;
    long n = server.arg("steps").toInt();
    if (n == 0 || labs(n) > cfg.stepsPerRev) { server.send(400, "text/plain", "Ogiltigt antal steg"); return; }
    enterManual(false, 0);
    jog(n);
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/setzero", HTTP_POST, []() {
    if (busy()) return;
    if (!homed) { server.send(400, "text/plain", "Kör 'Hitta magnet & gå till 0' först"); return; }
    cfg.zeroOffset = modp(stepper->currentPosition(), cfg.stepsPerRev);
    saveConfig();
    shownDialValue = 0;
    targetDialValue = 0;
    manualMs = millis();
    server.send(200, "text/plain", "Nollpunkt sparad: " + String(cfg.zeroOffset) + " steg från magneten");
  });

  server.on("/api/config", HTTP_POST, []() {
    if (busy()) return;
    bool oldInv = cfg.invertDir;
    int spr = server.arg("spr").toInt();
    int spd = server.arg("speed").toInt();
    float offs = server.arg("offseason").toFloat();
    if (spr >= 1000 && spr <= 10000) cfg.stepsPerRev = spr;
    if (spd >= 50 && spd <= 1200)    cfg.speed = spd;
    if (offs >= 0 && offs <= 4)      cfg.offseasonValue = offs;
    if (server.arg("region").length() >= 5) strlcpy(cfg.region, server.arg("region").c_str(), sizeof(cfg.region));
    cfg.invertDir     = server.arg("invert") == "1";
    cfg.hallActiveLow = server.arg("hallLow") == "1";
    saveConfig();
    shownDialValue = -1;     // visa om med nya inställningar
    fetchRequested = true;
    if (oldInv != cfg.invertDir) {
      server.send(200, "text/plain", "Sparat – startar om för ny rotationsriktning. Kalibrera om nollpunkten efteråt.");
      delay(500);
      ESP.restart();
    }
    server.send(200, "text/plain", "Sparat");
  });

  ElegantOTA.begin(&server);
  ElegantOTA.onStart([]() {
    if (stepper) stepper->disableOutputs();
    mState = M_IDLE;
  });
  server.begin();
}

// =====================================================================
//  Setup / loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nLavinmätare " FW_VERSION);
  pinMode(HALL_PIN, INPUT);

  LittleFS.begin();
  loadConfig();

  // Omvänd pinordning = omvänd rotationsriktning
  if (cfg.invertDir)
    stepper = new AccelStepper(AccelStepper::HALF4WIRE, PIN_IN4, PIN_IN2, PIN_IN3, PIN_IN1);
  else
    stepper = new AccelStepper(AccelStepper::HALF4WIRE, PIN_IN1, PIN_IN3, PIN_IN2, PIN_IN4);
  stepper->setMaxSpeed(cfg.speed);
  stepper->setAcceleration(cfg.speed);
  stepper->disableOutputs();

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setHostname(HOSTNAME);
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("Lavinmatare-Setup")) ESP.restart();

  configTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  setupWeb();
  Serial.println("Webb: http://" + WiFi.localIP().toString() + "/");
}

void loop() {
  server.handleClient();
  ElegantOTA.loop();
  MDNS.update();
  motorLoop();

  if (manual && millis() - manualMs > MANUAL_TIMEOUT_MS) {
    manual = false;
    testActive = false;
  }

  if (mState == M_IDLE) {
    // Hämta bara när motorn står still (TLS blockerar några sekunder)
    if (fetchRequested || millis() - lastFetchMs >= FETCH_INTERVAL_MS) doFetch();

    // Rör bara motorn när visat värde skiljer sig från önskat: homing + kör framåt till målet
    float d = desiredDial();
    if (!isnan(d) && !motorFault && fabsf(d - shownDialValue) > 0.001f) {
      targetDialValue = d;
      startHoming(true);
    }
  }
}
