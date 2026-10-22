#include <WiFi.h>
#include <WiFiClientSecure.h>  // Needed for HTTPS/TLS.............
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

// === Configuration ===
#define RELAY_PIN1 4
#define RELAY_PIN2 5
#define RELAY_ON LOW  // for low‐active relay modules
#define RELAY_OFF HIGH

const int v1 = 22, v2 = 18, v3 = 23, v4 = 27;      // Vibration motor GPIOs
const int np1 = 34, np2 = 35, np3 = 32, np4 = 33;  // NTC ADC pins

const float series_R = 9800.0;  // Fixed resistor in the voltage divider (Ω)
const float ntc_T = 25.0;       // Reference temperature for NTC (°C)
const float ntc_R = 18000.0;    // NTC resistance at reference temperature (Ω)
const float beta = 4100.0;      // Beta coefficient of the NTC
const int adcMax = 4095;        // 12-bit ADC on ESP32

// Replace with your actual Flask server URL. Because you are using HTTPS on Render,
// keep "https://…". If you switch your Flask instance to HTTP only, change to "http://…".
const char* serverUrl = "";//Paste your server link..

int reqTemp = -1;  // “requested temperature” from the last command (°C)
unsigned long lastFetchTime = 0;
const unsigned long fetchInterval = 5000;  // 5 seconds

// Replace these with your actual Wi-Fi SSID & password
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
  Serial.begin(115200);

  // — Initialize relays & vibration pins —
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  digitalWrite(RELAY_PIN1, RELAY_OFF);
  digitalWrite(RELAY_PIN2, RELAY_OFF);

  pinMode(v1, OUTPUT);
  pinMode(v2, OUTPUT);
  pinMode(v3, OUTPUT);
  pinMode(v4, OUTPUT);
  digitalWrite(v1, LOW);
  digitalWrite(v2, LOW);
  digitalWrite(v3, LOW);
  digitalWrite(v4, LOW);

  // — Connect to Wi-Fi —
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  float ptc1, ptc2, ptc3, ptc4;
  bool sysRun;
  // 1️⃣  Read four NTC thermistors, convert ADC → temperature (°C), then subtract 46.42 for calibration
  if (ptc1 < 60 && ptc2 < 60 && ptc3 < 60 && ptc4 < 60 && ptc1 > 0 && ptc2 > 0 && ptc3 > 0 && ptc4 > 0) {
    ptc1 = A2V2R2T(analogRead(np1)) - 46.42;
    ptc2 = A2V2R2T(analogRead(np2)) - 46.42;
    ptc3 = A2V2R2T(analogRead(np3)) - 46.42;
    ptc4 = A2V2R2T(analogRead(np4)) - 46.42;
    sysRun = true;
  } else {
    digitalWrite(RELAY_PIN1, RELAY_OFF);
    digitalWrite(RELAY_PIN2, RELAY_OFF);
    Serial.println("Unusuality Detected Both Relays Offed");
    sysRun = false;
  }
  // 3️⃣  Every fetchInterval (5 seconds), contact the server and run the command
  if (millis() - lastFetchTime >= fetchInterval) {
    fetchAndExecuteCommand(ptc1, ptc2, ptc3, ptc4, sysRun);
    lastFetchTime = millis();
  }
  Serial.print("PTC1: ");
  Serial.print(ptc1);
  Serial.print(" | PTC2: ");
  Serial.print(ptc2);
  Serial.print(" | PTC3: ");
  Serial.print(ptc3);
  Serial.print(" | PTC4: ");
  Serial.println(ptc4);
}

void fetchAndExecuteCommand(float ptc1, float ptc2, float ptc3, float ptc4, bool SysRun) {
  // — Make sure Wi-Fi is still connected —
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected.");
    return;
  }

  // — Build the correct URL (server listens on "/api") —
  String url = String(serverUrl) + "/api"
               + "?avgLeft=" + String((ptc1+ptc2)/2.0, 2)
               + "&avgRight=" + String((ptc3+ptc4)/2.0, 2);

  Serial.print("🌐 Requesting: ");
  Serial.println(url);

  // — Use a TLS client (HTTPS). For HTTP-only server, you could skip WiFiClientSecure.
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate validation (for testing)

  HTTPClient http;
  http.begin(client, url);  // IMPORTANT: use the secure client here

  int httpCode = http.GET();
  Serial.printf("HTTP code: %d\n", httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("📨 Response payload:");
    Serial.println(payload);

    // — Parse the JSON payload —
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("❌ JSON parse error: ");
      Serial.println(err.c_str());
      http.end();
      return;  // Abort if JSON was malformed
    }

    // — Extract fields safely —
    String command = doc["command"].as<const char*>();
    String status = doc["status"].as<const char*>();

    int degree = -1;
    if (doc.containsKey("degree") && doc["degree"].is<int>()) {
      degree = doc["degree"].as<int>();
    }

    Serial.print("Command: ");
    Serial.println(command);
    Serial.print("Degree : ");
    Serial.println(degree);
    Serial.print("Status : ");
    Serial.println(status);

    // — Only proceed if status == "success" —
    if (status == "success") {
      if (degree >= 0) {
        reqTemp = degree + 5;
      }
      // === Relay Control (requires a valid temperature setpoint) ===
      if (SysRun && ptc1 > 1 && ptc2 > 1 && ptc3 > 1 && ptc4 > 1) {
        if ((command == "tl" || command == "tr" || command == "tb" || command == "TL" || command == "TR" || command == "TB") && (reqTemp >= 0)) {
          if (command == "tl" || command == "tb") {
            bool turnOn1 = (ptc1 < reqTemp && ptc2 < reqTemp);
            digitalWrite(RELAY_PIN1, turnOn1 ? RELAY_ON : RELAY_OFF);
            Serial.println(turnOn1 ? "✅ Relay 1 ON" : "❌ Relay 1 OFF");
          }
          if (command == "TL" || command == "TB") {
            digitalWrite(RELAY_PIN1, RELAY_OFF);
            Serial.println("Relay 1 OFF");
          }
          if (command == "tr" || command == "tb") {
            bool turnOn2 = (ptc3 < reqTemp && ptc4 < reqTemp);
            digitalWrite(RELAY_PIN2, turnOn2 ? RELAY_ON : RELAY_OFF);
            Serial.println(turnOn2 ? "✅ Relay 2 ON" : "❌ Relay 2 OFF");
          }
          if (command == "TR" || command == "TB") {
            digitalWrite(RELAY_PIN2, RELAY_OFF);
            Serial.println("Relay 2 OFF");
          }
        }
      }
      // === Vibration Control (no temperature needed) ===
      if (command == "vl") {
        digitalWrite(v1, HIGH);
        digitalWrite(v2, HIGH);
        Serial.println("✅ Vibration LEFT ON");
      } else if (command == "VL") {
        digitalWrite(v1, LOW);
        digitalWrite(v2, LOW);
        Serial.println("✅ Vibration LEFT OFF");
      } else if (command == "vr") {
        digitalWrite(v3, HIGH);
        digitalWrite(v4, HIGH);
        Serial.println("✅ Vibration RIGHT ON");
      } else if (command == "VR") {
        digitalWrite(v3, LOW);
        digitalWrite(v4, LOW);
        Serial.println("✅ Vibration RIGHT OFF");
      } else if (command == "vb") {
        digitalWrite(v1, HIGH);
        digitalWrite(v2, HIGH);
        digitalWrite(v3, HIGH);
        digitalWrite(v4, HIGH);
        Serial.println("✅ Vibration BOTH ON");
      } else if (command == "VB") {
        digitalWrite(v1, LOW);
        digitalWrite(v2, LOW);
        digitalWrite(v3, LOW);
        digitalWrite(v4, LOW);
        Serial.println("✅ Vibration BOTH OFF");
      }
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }

  http.end();
}

// Convert raw ADC reading to temperature (Steinhart–Hart), then return °C
float A2V2R2T(int rawADC) {
  // Protect against division by zero
  if (rawADC <= 0) {
    return -273.15;  // invalid reading
  }
  float voltage = (rawADC * 3.3 / adcMax);
  float R_ntc = series_R * ((3.3 - voltage) / voltage);
  return steinhart(R_ntc);
}

float steinhart(float R_vol) {
  // Steinhart–Hart approximation
  float lnR = log(R_vol / ntc_R);
  float invT = (1.0 / (ntc_T + 273.15)) + (lnR / beta);
  float tempK = 1.0 / invT;
  return (tempK - 273.15);
}