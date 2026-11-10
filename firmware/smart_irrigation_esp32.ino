#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ============== WIFI CREDENTIALS ==============
// Replace with your 2.4 GHz hotspot/Wi-Fi details
const char* ssid = "312_Jio";
const char* password = "YOUR_WIFI_PASSWORD";

// ============== CLOUD CONFIGURATION (RENDER / VERCEL) ==============
// Once you deploy your backend on Render, paste your URL here:
// Example: "https://smart-irrigation-cloud.onrender.com/api/telemetry"
// Leave as "" to run purely in local mode.
const char* cloudServerUrl = "https://iot-bmwg.onrender.com/api/telemetry";

// ============== PIN CONFIGURATION ==============

#define DHTPIN 15
#define DHTTYPE DHT11

#define MOISTURE_PIN 32
#define RELAY_PIN 26

// ============== COMPONENTS ==============

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

WebServer server(80);

// ============== MOISTURE CALIBRATION ==============

int dryValue = 3500;
int wetValue = 1500;

// ============== PUMP SETTINGS ==============

const int ON_THRESHOLD = 30;
const int OFF_THRESHOLD = 90;

// Assumed active LOW relay
const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH;

bool pumpState = false;
bool automaticMode = true;
bool manualPumpState = false;

// ============== SENSOR VARIABLES ==============

float temperature = 0;
int moistureValue = 0;
int moisturePercent = 0;

bool temperatureValid = false;
bool moistureSensorValid = false;

// ============== TIMING ==============

unsigned long previousSensorMillis = 0;
const unsigned long sensorInterval = 2500;

// ============== RELAY CONTROL ==============

void updatePump() {
  digitalWrite(RELAY_PIN, pumpState ? RELAY_ON : RELAY_OFF);
}

// ============== PUMP LOGIC ==============

void controlPump() {

  if (!moistureSensorValid) {
    pumpState = false;
  }
  else if (automaticMode) {

    if (moisturePercent < ON_THRESHOLD) {
      pumpState = true;
    }
    else if (moisturePercent >= OFF_THRESHOLD) {
      pumpState = false;
    }

    // Retain previous state between thresholds (hysteresis)
  }
  else {
    pumpState = manualPumpState;
  }

  updatePump();
}

// ============== LCD DISPLAY ==============

void updateLCD() {

  lcd.setCursor(0, 0);

  if (temperatureValid) {
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.write(byte(223));
    lcd.print("C ");
  }
  else {
    lcd.print("T:Error ");
  }

  lcd.print(pumpState ? "P:ON " : "P:OFF");
  lcd.print("   ");

  lcd.setCursor(0, 1);

  if (moistureSensorValid) {
    lcd.print("Soil:");
    lcd.print(moisturePercent);
    lcd.print("% ");
  }
  else {
    lcd.print("Soil:ERROR ");
  }

  lcd.print(automaticMode ? "AUTO" : "MAN ");
}

// ============== SENSOR READING ==============

void readSensors() {

  temperature = dht.readTemperature();
  temperatureValid = !isnan(temperature);

  long total = 0;

  for (int i = 0; i < 5; i++) {
    total += analogRead(MOISTURE_PIN);
  }

  moistureValue = total / 5;

  moistureSensorValid =
    (moistureValue > 50 && moistureValue < 4050);

  if (moistureSensorValid) {

    moisturePercent = map(
      moistureValue,
      dryValue,
      wetValue,
      0,
      100
    );

    moisturePercent = constrain(
      moisturePercent,
      0,
      100
    );
  }
  else {
    moisturePercent = 0;
  }

  controlPump();
  updateLCD();

  Serial.println("---------------------------");

  if (temperatureValid) {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");
  }
  else {
    Serial.println("Temperature: ERROR");
  }

  Serial.print("Raw Moisture: ");
  Serial.println(moistureValue);

  Serial.print("Moisture: ");
  Serial.println(
    moistureSensorValid ?
    String(moisturePercent) + "%" :
    "Sensor Error"
  );

  Serial.print("Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");

  Serial.print("Mode: ");
  Serial.println(automaticMode ? "AUTO" : "MANUAL");
}

// ============== CLOUD SYNCHRONIZATION ==============

void syncWithCloud() {
  if (WiFi.status() != WL_CONNECTED || strlen(cloudServerUrl) == 0) return;

  WiFiClientSecure client;
  client.setInsecure(); // Bypass TLS certificate check for lightweight cloud connectivity

  HTTPClient http;
  if (http.begin(client, cloudServerUrl)) {
    http.setTimeout(3500); // 3.5s timeout prevents blocking main loop
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"temperature\":" + String(temperatureValid ? temperature : 0, 1) + ",";
    payload += "\"temperatureValid\":" + String(temperatureValid ? "true" : "false") + ",";
    payload += "\"moisture\":" + String(moisturePercent) + ",";
    payload += "\"moistureValid\":" + String(moistureSensorValid ? "true" : "false") + ",";
    payload += "\"rawMoisture\":" + String(moistureValue) + ",";
    payload += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
    payload += "\"mode\":\"" + String(automaticMode ? "AUTO" : "MANUAL") + "\"";
    payload += "}";

    int httpCode = http.POST(payload);
    if (httpCode == 200 || httpCode == 201) {
      String response = http.getString();

      if (response.indexOf("\"mode\":\"MANUAL\"") >= 0) {
        automaticMode = false;
      } else if (response.indexOf("\"mode\":\"AUTO\"") >= 0) {
        automaticMode = true;
      }

      if (!automaticMode) {
        if (response.indexOf("\"manualPump\":true") >= 0) {
          manualPumpState = true;
        } else if (response.indexOf("\"manualPump\":false") >= 0) {
          manualPumpState = false;
        }
      }
      controlPump();
      updateLCD();
    }
    http.end();
    client.stop(); // Immediately release TLS socket memory and prevent heap exhaustion
  }
}

// ============== WEB DASHBOARD ==============

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Irrigation System</title>
<style>
* { box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: #f0f4f8;
  margin: 0;
  padding: 16px;
  color: #1a202c;
}
.container {
  max-width: 800px;
  margin: 0 auto;
}
header {
  text-align: center;
  margin-bottom: 24px;
}
h1 {
  color: #1b4332;
  margin: 0 0 6px 0;
  font-size: 26px;
}
.subtitle {
  color: #555;
  margin: 0;
  font-size: 14px;
}
#connection {
  display: inline-block;
  padding: 6px 14px;
  border-radius: 20px;
  font-size: 13px;
  font-weight: 600;
  margin-top: 10px;
  background: #e2e8f0;
  color: #4a5568;
}
#connection.connected {
  background: #d4edda;
  color: #155724;
}
#connection.disconnected {
  background: #f8d7da;
  color: #721c24;
}
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: 16px;
  margin-bottom: 20px;
}
.card {
  background: #ffffff;
  padding: 20px 16px;
  border-radius: 12px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.05);
  text-align: center;
  border: 1px solid #e9ecef;
}
.card h3 {
  color: #6c757d;
  font-size: 14px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  margin: 0 0 10px 0;
}
.value {
  font-size: 32px;
  font-weight: 700;
  color: #2d3748;
}
.status {
  font-size: 24px;
  font-weight: 700;
}
.status.on { color: #2e7d32; }
.status.off { color: #c62828; }
.status.auto { color: #1565c0; }
.status.manual { color: #e65100; }
.controls {
  background: white;
  padding: 22px;
  border-radius: 12px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.05);
  border: 1px solid #e9ecef;
  text-align: center;
}
.controls h2 {
  font-size: 20px;
  color: #1b4332;
  margin-top: 0;
  margin-bottom: 16px;
}
.control-group {
  margin-bottom: 20px;
}
.control-group h4 {
  margin: 0 0 10px 0;
  color: #495057;
  font-size: 15px;
}
.btn-row {
  display: flex;
  gap: 10px;
  justify-content: center;
  flex-wrap: wrap;
}
button {
  padding: 12px 22px;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  font-size: 15px;
  font-weight: 600;
  color: white;
  background: #2d6a4f;
  transition: all 0.2s;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}
button:hover {
  opacity: 0.9;
  transform: translateY(-1px);
}
button:active {
  transform: translateY(1px);
}
button.off {
  background: #d90429;
}
button.secondary {
  background: #4a5568;
}
button:disabled {
  background: #cbd5e0;
  cursor: not-allowed;
  transform: none;
}
#message {
  margin-top: 14px;
  font-size: 14px;
  min-height: 20px;
  font-weight: 500;
  color: #2b6cb0;
}
</style>
</head>
<body>
<div class="container">
  <header>
    <h1>Smart Irrigation System</h1>
    <p class="subtitle">ESP32 Live IoT Dashboard</p>
    <div id="connection">Connecting to ESP32...</div>
  </header>

  <div class="grid">
    <div class="card">
      <h3>Temperature</h3>
      <div class="value" id="temperature">-- °C</div>
    </div>
    <div class="card">
      <h3>Soil Moisture</h3>
      <div class="value" id="moisture">-- %</div>
    </div>
    <div class="card">
      <h3>Raw Moisture ADC</h3>
      <div class="value" id="raw">--</div>
    </div>
    <div class="card">
      <h3>Pump Status</h3>
      <div class="status" id="pump">--</div>
    </div>
    <div class="card">
      <h3>Operating Mode</h3>
      <div class="status" id="mode">--</div>
    </div>
  </div>

  <div class="controls">
    <h2>System Controls</h2>

    <div class="control-group">
      <h4>Operating Mode</h4>
      <div class="btn-row">
        <button onclick="setMode('auto')">Automatic Mode</button>
        <button class="secondary" onclick="setMode('manual')">Manual Mode</button>
      </div>
    </div>

    <div class="control-group">
      <h4>Manual Pump Control</h4>
      <div class="btn-row">
        <button id="btnPumpOn" onclick="setPump(1)">Pump ON</button>
        <button id="btnPumpOff" class="off" onclick="setPump(0)">Pump OFF</button>
      </div>
    </div>

    <div id="message"></div>
  </div>
</div>

<script>
let isAutoMode = true;

async function updateDashboard() {
  try {
    const response = await fetch('/data');
    if (!response.ok) throw new Error("HTTP error");
    const data = await response.json();

    document.getElementById('temperature').innerText =
      data.temperatureValid ? data.temperature + ' °C' : 'Error';

    document.getElementById('moisture').innerText =
      data.moistureValid ? data.moisture + ' %' : 'Error';

    document.getElementById('raw').innerText = data.rawMoisture;

    const pumpElem = document.getElementById('pump');
    pumpElem.innerText = data.pump ? 'ON' : 'OFF';
    pumpElem.className = 'status ' + (data.pump ? 'on' : 'off');

    const modeElem = document.getElementById('mode');
    modeElem.innerText = data.mode;
    modeElem.className = 'status ' + (data.mode === 'AUTO' ? 'auto' : 'manual');

    isAutoMode = (data.mode === 'AUTO');
    document.getElementById('btnPumpOn').disabled = isAutoMode;
    document.getElementById('btnPumpOff').disabled = isAutoMode;

    const conn = document.getElementById('connection');
    conn.innerText = 'Connected to ESP32';
    conn.className = 'connected';
  } catch(error) {
    const conn = document.getElementById('connection');
    conn.innerText = 'Connection Lost - Retrying...';
    conn.className = 'disconnected';
  }
}

async function setMode(mode) {
  try {
    const response = await fetch('/mode?value=' + mode);
    const msg = await response.text();
    document.getElementById('message').innerText = msg;
    updateDashboard();
  } catch(error) {
    document.getElementById('message').innerText = 'Could not update mode';
  }
}

async function setPump(state) {
  try {
    const response = await fetch('/pump?state=' + state);
    const msg = await response.text();
    document.getElementById('message').innerText = msg;
    updateDashboard();
  } catch(error) {
    document.getElementById('message').innerText = 'Could not control pump';
  }
}

updateDashboard();
setInterval(updateDashboard, 2500);
</script>
</body>
</html>
)rawliteral";

// ============== API: SENSOR DATA ==============

void handleData() {

  String json = "{";

  json += "\"temperature\":" +
          String(temperatureValid ? temperature : 0, 1) + ",";

  json += "\"temperatureValid\":" +
          String(temperatureValid ? "true" : "false") + ",";

  json += "\"moisture\":" +
          String(moisturePercent) + ",";

  json += "\"moistureValid\":" +
          String(moistureSensorValid ? "true" : "false") + ",";

  json += "\"rawMoisture\":" +
          String(moistureValue) + ",";

  json += "\"pump\":" +
          String(pumpState ? "true" : "false") + ",";

  json += "\"mode\":\"" +
          String(automaticMode ? "AUTO" : "MANUAL") + "\"";

  json += "}";

  server.send(200, "application/json", json);
}

// ============== API: MODE CONTROL ==============

void handleMode() {

  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "Missing mode parameter");
    return;
  }

  String value = server.arg("value");

  if (value == "auto") {
    automaticMode = true;
  }
  else if (value == "manual") {
    automaticMode = false;
  }
  else {
    server.send(400, "text/plain", "Invalid mode");
    return;
  }

  controlPump();
  updateLCD();

  server.send(200, "text/plain", "Mode switched to " + String(automaticMode ? "AUTO" : "MANUAL"));
}

// ============== API: PUMP CONTROL ==============

void handlePump() {

  if (!server.hasArg("state")) {
    server.send(400, "text/plain", "Missing pump state");
    return;
  }

  if (automaticMode) {
    server.send(409, "text/plain", "Please switch to MANUAL mode first");
    return;
  }

  String value = server.arg("state");

  if (value == "1") {
    manualPumpState = true;
  }
  else if (value == "0") {
    manualPumpState = false;
  }
  else {
    server.send(400, "text/plain", "Invalid pump state");
    return;
  }

  controlPump();
  updateLCD();

  server.send(200, "text/plain", "Pump turned " + String(manualPumpState ? "ON" : "OFF"));
}

// ============== SETUP ==============

void setup() {

  Serial.begin(115200);

  // Relay OFF during startup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Smart Irrigation");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  dht.begin();
  analogReadResolution(12);

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); // Disable Wi-Fi modem sleep to prevent random disconnects
  WiFi.setTxPower(WIFI_POWER_17dBm); // Stable transmission power, prevents RF saturation near phone
  WiFi.begin(ssid, password);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  Serial.print("Connecting to WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("\n====================");
    Serial.println("WIFI CONNECTED!");
    Serial.println("====================");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Start mDNS (allows accessing via http://irrigation.local)
    if (MDNS.begin("irrigation")) {
      Serial.println("mDNS responder active: http://irrigation.local");
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");

    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());

  }
  else {

    Serial.println("\nWiFi Connection Waiting... ESP32 will auto-connect in background.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Waiting...");

  }

  readSensors();

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", webpage);
  });

  server.on("/data", HTTP_GET, handleData);
  server.on("/mode", HTTP_GET, handleMode);
  server.on("/pump", HTTP_GET, handlePump);

  server.begin();

  Serial.println("Web server started successfully!");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("-------------------------------------");
    Serial.print("Open in Browser: http://");
    Serial.println(WiFi.localIP());
    Serial.println("Or via mDNS:    http://irrigation.local");
    Serial.println("-------------------------------------");
  }

}

// ============== MAIN LOOP ==============

void loop() {

  server.handleClient();

  // Serial commands for testing manual mode directly over USB
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "auto") {
      automaticMode = true;
      controlPump();
      updateLCD();
      Serial.println(">> MODE SWITCHED TO: AUTO");
    }
    else if (cmd == "manual") {
      automaticMode = false;
      controlPump();
      updateLCD();
      Serial.println(">> MODE SWITCHED TO: MANUAL");
    }
    else if (cmd == "on" || cmd == "pump on" || cmd == "1") {
      if (automaticMode) {
        Serial.println(">> Cannot turn on: Currently in AUTO mode. Send 'manual' first!");
      }
      else {
        manualPumpState = true;
        controlPump();
        updateLCD();
        Serial.println(">> PUMP TURNED ON (MANUAL)");
      }
    }
    else if (cmd == "off" || cmd == "pump off" || cmd == "0") {
      if (automaticMode) {
        Serial.println(">> Cannot turn off: Currently in AUTO mode. Send 'manual' first!");
      }
      else {
        manualPumpState = false;
        controlPump();
        updateLCD();
        Serial.println(">> PUMP TURNED OFF (MANUAL)");
      }
    }
    else if (cmd == "status") {
      Serial.println(">> Status: Mode=" + String(automaticMode ? "AUTO" : "MANUAL") +
                     " | Pump=" + String(pumpState ? "ON" : "OFF") +
                     " | Soil=" + String(moisturePercent) + "%");
    }
  }

  // Non-intrusive auto-reconnect watchdog (only triggers if connection is genuinely lost for > 15 seconds)
  static unsigned long disconnectedSince = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (disconnectedSince == 0) {
      disconnectedSince = millis();
    } else if (millis() - disconnectedSince > 15000) {
      disconnectedSince = millis();
      Serial.println(">> Wi-Fi lost for 15s. Reconnecting...");
      WiFi.reconnect();
    }
  } else {
    disconnectedSince = 0;
  }

  if (millis() - previousSensorMillis >= sensorInterval) {

    previousSensorMillis = millis();
    readSensors();

  }

  // Cloud sync timer: Dispatches HTTPS telemetry every 5 seconds (prevents memory fragmentation)
  static unsigned long previousCloudMillis = 0;
  if (millis() - previousCloudMillis >= 5000) {
    previousCloudMillis = millis();
    syncWithCloud();
  }

}

