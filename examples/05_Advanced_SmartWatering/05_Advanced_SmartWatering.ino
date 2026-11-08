/*
  IoTLabs Advanced 05 — Smart Watering

  Additional Arduino Library Manager dependencies:
    DHT sensor library by Adafruit
    Adafruit GFX Library
    Adafruit SSD1306

  Automated plant watering on soil-moisture hysteresis, monitored and
  controlled end-to-end through IoTLabs Console (web) and the IoTLabs Maker
  mobile app: temperature/humidity/soil readings, adjustable ON/OFF soil
  thresholds, and a single Pump control, all rendered from the same Control
  Type Registry entries both apps read — see PROFILE_INTENT.json for the
  exact Control mapping.

  Control model: there is no client-facing "mode" switch. Pump is one
  composite Control (control_4_auto_or_manual, {"state","source"}) — the
  device runs its own AUTO hysteresis and only ever falls out of it when a
  client writes "state" (Console/Mobile) or the physical button is pressed,
  both treated as the same "manual override" event, auto-releasing back to
  AUTO after MANUAL_OVERRIDE_MS. See
  IoTLabs internal design notes

  ESP32 Dev Kit V1 pins:
    OLED SDA    GPIO21
    OLED SCL    GPIO22
    Button      GPIO25 -> GND, INPUT_PULLUP  (pump toggle)
    Relay       GPIO26, Active HIGH
    DHT11       GPIO27
    Soil (ADC)  GPIO34
    BOOT        GPIO0                        (long-press factory reset)

  Two things this example adds in application code, on top of the library's
  own provisioning/Control primitives — see README.md "Các use case thực tế
  đã xử lý trong code" for the full writeup:
    1. Long-press-to-factory-reset. factoryReset() exists on IoTLabs, but
       the hold-timing is 100% application code below (updateBootButton()).
    2. An HTML form for /provision — the library's SoftAP serves bare JSON
       REST only. provisioning_page.h adds a small HTML form for this
       example so a phone browser can drive WiFi setup directly, no REST
       client app needed.
*/

// If pointing this at a *local* device-platform for development: its HTTP
// client (Esp32ArduinoHttpClient.cpp) always uses WiFiClientSecure — there
// is no plain-HTTP path (tlsInsecure only skips certificate *validation*,
// it doesn't drop TLS itself, and isn't exposed as a sketch macro here — see
// IoTLabs.h's begin(config) for the advanced path). A plain http:// dev
// backend is unreachable from real firmware as-is; this example's real test
// run put a throwaway local TLS relay (self-signed cert, tlsInsecure=true
// via the advanced config accepts it) in front of it instead — see
// README.md "Testing notes" for the exact setup and real IDs used.
#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include "provisioning_page.h"

constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
constexpr uint8_t BUTTON_PIN = 25;
constexpr uint8_t RELAY_PIN = 26;
constexpr uint8_t DHT_PIN = 27;
constexpr uint8_t SOIL_PIN = 34;
constexpr uint8_t BOOT_BUTTON_PIN = 0;
constexpr uint8_t DHT_TYPE = DHT11;

// true = relay module is active-LOW (IN=LOW energizes it). Must match the
// Low/High Level Trigger jumper physically set on the relay module.
constexpr bool RELAY_ACTIVE_LOW = false;

constexpr char CONTROL_TEMPERATURE[] = "control_1_value";
constexpr char CONTROL_HUMIDITY[] = "control_2_value";
constexpr char CONTROL_SOIL[] = "control_3_value";
// Composite auto_or_manual Control — {"state":"on"|"off","source":"auto"|"manual"}.
// Replaces the old separate control_4_select (Watering Mode) + control_5_on_off
// (Pump) pair: there is no client-writable "mode" anymore, only "state". Any
// client write to "state" — dashboard, mobile app, or the physical button —
// is by definition a manual override; "source" is device-reported only. See
// IoTLabs internal design notes
constexpr char CONTROL_PUMP[] = "control_4_auto_or_manual";
constexpr char CONTROL_THRESH_ON[] = "control_5_value";
constexpr char CONTROL_THRESH_OFF[] = "control_6_value";
constexpr char CONTROL_BUTTON[] = "control_7_button";

constexpr uint32_t SENSOR_INTERVAL_MS = 2000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 300;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
// How long a manual override (dashboard/app write or physical button) locks
// AUTO's hysteresis out before the device silently resumes AUTO on its own.
// Without this, a forgotten manual ON/OFF sticks forever with no local
// safety net — see PRD "Vấn đề có thể gặp phải" #1.
constexpr uint32_t MANUAL_OVERRIDE_MS = 15UL * 60UL * 1000UL;  // 15 minutes

// Calibration placeholders — re-calibrate per physical sensor
// (dry raw ADC, wet raw ADC).
constexpr int SOIL_RAW_DRY = 3200;
constexpr int SOIL_RAW_WET = 1100;
constexpr int SOIL_ON_THRESHOLD_DEFAULT = 35;
constexpr int SOIL_OFF_THRESHOLD_DEFAULT = 60;

// NVS namespace for persisting user-set thresholds across reboots — plain
// globals alone reset to the compile-time defaults on every power cycle.
constexpr char PREFS_NAMESPACE[] = "smartwater";
constexpr char PREFS_KEY_SOIL_ON[] = "soilOn";
constexpr char PREFS_KEY_SOIL_OFF[] = "soilOff";

enum class PumpMode { AUTO, MANUAL };

IoTLabs iot;
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Preferences prefs;

float temperature = NAN;
float humidity = NAN;
int soilRaw = 0;
int soilPercent = 0;
bool pumpOn = false;
PumpMode pumpMode = PumpMode::AUTO;
// millis() timestamp when an active MANUAL override auto-releases back to
// AUTO; meaningless while pumpMode == AUTO.
uint32_t manualOverrideUntil = 0;
bool oledReady = false;
int soilOnThreshold = SOIL_ON_THRESHOLD_DEFAULT;
int soilOffThreshold = SOIL_OFF_THRESHOLD_DEFAULT;
// Set from the MQTT command callback, actually written to flash from loop()
// — an NVS commit is a synchronous flash erase+write, and doing that inline
// inside the command callback (while the radio is mid-TX acking the MQTT
// publish) was enough current draw to brownout-reset the board on real
// hardware. Deferring to loop() moves the write off that hot path.
bool soilThresholdsDirty = false;

bool lastButtonRaw = HIGH;
bool buttonStable = HIGH;
uint32_t lastButtonChangeAt = 0;

bool lastBootButtonRaw = HIGH;
uint32_t bootButtonDownAt = 0;
bool factoryResetFired = false;

uint32_t lastSensorAt = 0;
uint32_t lastDisplayAt = 0;

const char* modeString(PumpMode mode) {
  return mode == PumpMode::AUTO ? "auto" : "manual";
}

const char* eventName(IoTLabsEvent event) {
  switch (event) {
    case IoTLabsEvent::BOOT: return "BOOT";
    case IoTLabsEvent::PROVISIONING_STARTED: return "PROVISIONING_STARTED";
    case IoTLabsEvent::PROVISIONING_ACCEPTED: return "PROVISIONING_ACCEPTED";
    case IoTLabsEvent::WIFI_CONNECTING: return "WIFI_CONNECTING";
    case IoTLabsEvent::WIFI_CONNECTED: return "WIFI_CONNECTED";
    case IoTLabsEvent::WIFI_FAILED: return "WIFI_FAILED";
    case IoTLabsEvent::ACTIVATING: return "ACTIVATING";
    case IoTLabsEvent::ACTIVATED: return "ACTIVATED";
    case IoTLabsEvent::CLOUD_CONNECTING: return "CLOUD_CONNECTING";
    case IoTLabsEvent::ONLINE: return "ONLINE";
    case IoTLabsEvent::OFFLINE: return "OFFLINE";
    case IoTLabsEvent::PUBLISHED: return "PUBLISHED";
    case IoTLabsEvent::MESSAGE_ERROR: return "MESSAGE_ERROR";
    case IoTLabsEvent::ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

void writeRelay(bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  }
}

// Builds and reports the full {"state","source"} composite — the only report
// path for CONTROL_PUMP now (composite Controls always report the full
// object, never a partial one).
void reportPumpControl(bool publishNow = true) {
  JsonDocument doc;
  doc["state"] = pumpOn ? "on" : "off";
  doc["source"] = pumpMode == PumpMode::AUTO ? "auto" : "manual";
  iot.report(CONTROL_PUMP, doc.as<JsonVariantConst>(), publishNow);
}

// Relay + pumpOn only, no report. Shared by setPump()/enterManualOverride()
// so each of those does exactly one report per call instead of two.
void applyPumpState(bool on) {
  pumpOn = on;
  writeRelay(pumpOn);
}

// Single source of truth for pump state while AUTO owns the actuator —
// called only from the AUTO hysteresis logic (applyAutoControl()).
void setPump(bool on) {
  if (pumpOn == on) return;
  applyPumpState(on);
  reportPumpControl();
}

// Entry point for every MANUAL action — a cloud "state" write and the
// physical button both call this. Always reports, even if `state` happens
// not to change, because `source` flipping to "manual" is itself
// user-visible. Resets the auto-release timer on every call, so repeated
// manual actions keep extending the override instead of expiring mid-use.
void enterManualOverride(bool on) {
  pumpMode = PumpMode::MANUAL;
  manualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
  if (pumpOn != on) applyPumpState(on);
  reportPumpControl();
}

// Called every loop() iteration. Silently hands the actuator back to AUTO
// once the override window elapses — the local safety net against a
// forgotten manual ON/OFF (see PRD "Vấn đề có thể gặp phải" #1). Uses
// subtraction rather than direct comparison so it stays correct across a
// millis() rollover (~49.7 days).
void releaseManualOverrideIfExpired() {
  if (pumpMode != PumpMode::MANUAL) return;
  if (static_cast<int32_t>(millis() - manualOverrideUntil) < 0) return;
  pumpMode = PumpMode::AUTO;
  reportPumpControl();
}

// AUTO hysteresis: soil < onThreshold -> pump ON; soil >= offThreshold ->
// pump OFF; the band between the two thresholds holds whatever state the
// pump was already in (prevents relay chattering right at one threshold).
void applyAutoControl() {
  if (pumpMode != PumpMode::AUTO) return;

  if (soilPercent < soilOnThreshold && !pumpOn) {
    setPump(true);
  } else if (soilPercent >= soilOffThreshold && pumpOn) {
    setPump(false);
  }
}

int readSoilRawAverage() {
  long total = 0;
  constexpr int SAMPLE_COUNT = 10;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    total += analogRead(SOIL_PIN);
    delay(5);
  }
  return static_cast<int>(total / SAMPLE_COUNT);
}

int rawToSoilPercent(int raw) {
  int percent = map(raw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  return constrain(percent, 0, 100);
}

void printStatus() {
  Serial.print("[APP] T=");
  Serial.print(temperature);
  Serial.print("C H=");
  Serial.print(humidity);
  Serial.print("% SoilRaw=");
  Serial.print(soilRaw);
  Serial.print(" Soil=");
  Serial.print(soilPercent);
  Serial.print("% Pump=");
  Serial.print(pumpOn ? "ON" : "OFF");
  Serial.print(" Mode=");
  Serial.println(modeString(pumpMode));
}

void readSensorsAndUpdate() {
  const float h = dht.readHumidity();
  const float t = dht.readTemperature();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  soilRaw = readSoilRawAverage();
  soilPercent = rawToSoilPercent(soilRaw);

  applyAutoControl();

  if (!isnan(temperature)) iot.report(CONTROL_TEMPERATURE, temperature);
  if (!isnan(humidity)) iot.report(CONTROL_HUMIDITY, humidity);
  iot.report(CONTROL_SOIL, soilPercent);

  printStatus();
}

void updateDisplay() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("T:");
  if (isnan(temperature)) display.print("--"); else display.print(temperature, 1);
  display.print("C H:");
  if (isnan(humidity)) display.print("--"); else display.print(humidity, 0);
  display.println("%");

  display.setCursor(0, 18);
  display.print("Soil: ");
  display.print(soilPercent);
  display.print("% [");
  display.print(soilOnThreshold);
  display.print("-");
  display.print(soilOffThreshold);
  display.println("]");

  display.setCursor(0, 32);
  display.print("Pump: ");
  display.print(pumpOn ? "ON" : "OFF");
  display.print("  Mode: ");
  display.println(pumpMode == PumpMode::AUTO ? "AUTO" : "MANUAL");

  display.setCursor(0, 52);
  if (!iot.provisioned()) {
    display.print("Setup: ");
    display.println(iot.provisioningApName());
  } else {
    display.print("Cloud: ");
    display.println(iot.connected() ? "ONLINE" : "OFFLINE");
  }

  display.display();
}

// Physical pump button always wins — forces MANUAL first (so the state
// shown in Console/Mobile stays consistent with what just happened at the
// device), then toggles the pump. A physical press is a deliberate user
// action and is never "locked out" the way a stale UI state could be.
void triggerButtonAction() {
  enterManualOverride(!pumpOn);

  JsonDocument value;
  value["state"] = "pressed";
  value["action"] = "toggle_pump";
  iot.report(CONTROL_BUTTON, value.as<JsonVariantConst>());
}

void updateButton() {
  const bool raw = digitalRead(BUTTON_PIN);
  const uint32_t now = millis();

  if (raw != lastButtonRaw) {
    lastButtonRaw = raw;
    lastButtonChangeAt = now;
  }
  if (now - lastButtonChangeAt < BUTTON_DEBOUNCE_MS || raw == buttonStable) return;

  buttonStable = raw;
  if (buttonStable == LOW) triggerButtonAction();
}

// Long-press BOOT (10s) -> factory reset. IoTLabs Library ships
// factoryReset() but no hold-timing of its own — the 10s hold is
// implemented entirely in this application's code (see README.md "Các use
// case thực tế đã xử lý trong code").
void updateBootButton() {
  const bool raw = digitalRead(BOOT_BUTTON_PIN);
  const uint32_t now = millis();

  if (raw == LOW && lastBootButtonRaw == HIGH) {
    bootButtonDownAt = now;
    factoryResetFired = false;
  }

  if (raw == LOW && !factoryResetFired && (now - bootButtonDownAt) >= FACTORY_RESET_HOLD_MS) {
    factoryResetFired = true;
    Serial.println("[APP] BOOT held 10s -- factory reset");
    iot.factoryReset();  // restarts the device by default
  }

  lastBootButtonRaw = raw;
}

// controlTypeAutoOrManual command handler. There is no "set mode" command —
// AUTO is the default and only resumes on its own
// (releaseManualOverrideIfExpired()); every inbound "state" write, whether
// from Console/Mobile or the physical button, is a manual override by
// definition.
void onPumpControl(const IoTLabsValue& value) {
  if (!value.has("state")) {
    reportPumpControl();  // nothing actionable — echo current composite
    return;
  }
  const String state = value["state"].asString();
  if (state != "on" && state != "off") {
    reportPumpControl();  // reject unknown state, echo current composite
    return;
  }
  enterManualOverride(state == "on");
}

void onThresholdOn(float value) {
  const int v = constrain(static_cast<int>(value), 0, 100);
  if (v >= soilOffThreshold) {
    iot.report(CONTROL_THRESH_ON, soilOnThreshold);  // reject, echo old value
    return;
  }
  soilOnThreshold = v;
  soilThresholdsDirty = true;
  iot.report(CONTROL_THRESH_ON, soilOnThreshold);
}

void onThresholdOff(float value) {
  const int v = constrain(static_cast<int>(value), 0, 100);
  if (v <= soilOnThreshold) {
    iot.report(CONTROL_THRESH_OFF, soilOffThreshold);  // reject, echo old value
    return;
  }
  soilOffThreshold = v;
  soilThresholdsDirty = true;
  iot.report(CONTROL_THRESH_OFF, soilOffThreshold);
}

void onIoTLabsEvent(IoTLabsEvent event) {
  Serial.print("[EVENT] t=");
  Serial.print(millis());
  Serial.print("ms ");
  Serial.println(eventName(event));
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_PIN, OUTPUT);
  writeRelay(false);  // Safe boot: pump OFF.

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastButtonRaw = digitalRead(BUTTON_PIN);
  buttonStable = lastButtonRaw;

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  lastBootButtonRaw = digitalRead(BOOT_BUTTON_PIN);

  analogReadResolution(12);
  dht.begin();

  Wire.begin(OLED_SDA, OLED_SCL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledReady) Serial.println("[APP] OLED init failed");

  prefs.begin(PREFS_NAMESPACE, false);
  soilOnThreshold = prefs.getInt(PREFS_KEY_SOIL_ON, SOIL_ON_THRESHOLD_DEFAULT);
  soilOffThreshold = prefs.getInt(PREFS_KEY_SOIL_OFF, SOIL_OFF_THRESHOLD_DEFAULT);

  reportPumpControl(false);
  iot.report(CONTROL_THRESH_ON, soilOnThreshold, false);
  iot.report(CONTROL_THRESH_OFF, soilOffThreshold, false);

  iot.on(CONTROL_PUMP, onPumpControl);
  iot.on(CONTROL_THRESH_ON, onThresholdOn);
  iot.on(CONTROL_THRESH_OFF, onThresholdOff);
  iot.onEvent(onIoTLabsEvent);

  iot.begin();

  beginProvisioningPage();  // port 8080 HTML form -> proxies to /provision

  readSensorsAndUpdate();
  lastSensorAt = millis();
}

void loop() {
  iot.loop();
  pollProvisioningPage();

  // Local hardware remains responsive even when Cloud is offline.
  updateButton();
  updateBootButton();
  releaseManualOverrideIfExpired();

  if (soilThresholdsDirty) {
    soilThresholdsDirty = false;
    prefs.putInt(PREFS_KEY_SOIL_ON, soilOnThreshold);
    prefs.putInt(PREFS_KEY_SOIL_OFF, soilOffThreshold);
  }

  const uint32_t now = millis();

  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
    lastSensorAt = now;
    readSensorsAndUpdate();
  }

  if (now - lastDisplayAt >= DISPLAY_INTERVAL_MS) {
    lastDisplayAt = now;
    updateDisplay();
  }
}
