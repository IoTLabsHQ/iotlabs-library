/*
  IoTLabs 06 — Indoor Air Quality Monitor

  Additional Arduino Library Manager dependencies:
    SparkFun SCD4x Arduino Library (CO2 + temperature + humidity, I2C)
    Adafruit PM25 AQI Sensor        (PM1.0/PM2.5/PM10, UART)
    Adafruit GFX Library
    Adafruit SSD1306
    Adafruit NeoPixel

  V1 scope (see IoTLabs internal design notes
  PRD_Indoor Air Quality Monitor.md, section 4.1): CO2 + Temperature +
  Humidity from one SCD40/SCD41 is the required core (the "closed room"
  problem). PM2.5 from a PMS5003/PMSA003 is an OPTIONAL add-on (dust/smoke
  is a different problem than a stuffy closed room) — auto-detected at
  boot via pm25Sensor.begin_UART()'s return value; if absent, the device
  runs CO2-only and simply never reports control_2_value (silently absent
  from Console/Mobile, not an app bug — see
  iotlabs-device-platform/docs/000_5_CONTROL_WIDGET_SPEC.md's DHT11 worked
  example, point 5). Local OLED display, local GOOD/MODERATE/POOR rule (no
  cloud dependency), buzzer + status LED alert with mute, IoTLabs Cloud
  reporting, factory reset. No VOC/NOx/CO/GPS/battery/touchscreen — see
  PRD "Ngoai pham vi V1".

  ESP32 Dev Kit V1 pins (see docs/01_KET_NOI_PHAN_CUNG.md):
    OLED/SCD4x SDA  GPIO21   (shared I2C bus: SCD4x 0x62, OLED 0x3C)
    OLED/SCD4x SCL  GPIO22
    PM2.5 RX2       GPIO16   (optional module — ESP32 receives, wired to PM module TX)
    PM2.5 TX2       GPIO17   (spare, module runs in its own active/streaming mode)
    Mode/mute btn   GPIO27   INPUT_PULLUP, active LOW
    Buzzer          GPIO4    active HIGH
    Status LED      GPIO5    WS2812, 1 pixel
    BOOT            GPIO0    hold 10s -> factory reset

  Air Quality Status thresholds below are reference values only (WHO/US-EPA
  style indoor CO2/PM2.5 bands), not yet field-validated on real hardware —
  see PRD GIAI DOAN 03 (Test thuc te). Re-tune after real-world testing.

  Buzzer Enable / mute state is RAM-only and resets to defaults on reboot,
  same known gap as example 05_Advanced_SmartWatering's un-persisted app
  settings (see that example's README "Gaps vs. Blynk" #1).
*/

#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <Wire.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include <Adafruit_PM25AQI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t I2C_SDA = 21;
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t PM_RX_PIN = 16;
constexpr uint8_t PM_TX_PIN = 17;
constexpr uint8_t MUTE_BUTTON_PIN = 27;
constexpr uint8_t BUZZER_PIN = 4;
constexpr uint8_t STATUS_LED_PIN = 5;
constexpr uint8_t BOOT_BUTTON_PIN = 0;

constexpr char CONTROL_CO2[]         = "control_1_value";
constexpr char CONTROL_PM25[]        = "control_2_value";
constexpr char CONTROL_TEMPERATURE[] = "control_3_value";
constexpr char CONTROL_HUMIDITY[]    = "control_4_value";
constexpr char CONTROL_AIR_STATUS[]  = "control_5_text";
constexpr char CONTROL_BUZZER_EN[]   = "control_6_on_off";
constexpr char CONTROL_MUTE_BUTTON[] = "control_7_button";

// SCD4x's own periodic-measurement update interval is 5s; polling faster
// than that just re-reads stale data.
constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 500;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
// How long a mute (physical button or cloud command) silences the buzzer
// before it resumes alerting on its own if the room is still POOR.
constexpr uint32_t MUTE_DURATION_MS = 15UL * 60UL * 1000UL;  // 15 minutes

// Reference thresholds only, not yet field-validated — see file header.
constexpr uint16_t CO2_MODERATE_PPM = 1000;
constexpr uint16_t CO2_POOR_PPM = 2000;
constexpr float PM25_MODERATE_UGM3 = 12.0f;
constexpr float PM25_POOR_UGM3 = 35.0f;

enum class AirQuality { GOOD, MODERATE, POOR };

IoTLabs iot;
SCD4x scd4x;
Adafruit_PM25AQI pm25Sensor;
HardwareSerial pmSerial(2);
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

uint16_t co2 = 0;
float temperature = NAN;
float humidity = NAN;
uint16_t pm25 = 0;
bool scd4xReady = false;
bool pm25Ready = false;  // optional module — see file header, PRD 4.1
bool oledReady = false;
AirQuality airQuality = AirQuality::GOOD;

bool buzzerEnabled = true;
bool muted = false;
uint32_t mutedUntil = 0;

bool lastButtonRaw = HIGH;
bool buttonStable = HIGH;
uint32_t lastButtonChangeAt = 0;

bool lastBootButtonRaw = HIGH;
uint32_t bootButtonDownAt = 0;
bool factoryResetFired = false;

uint32_t lastSensorAt = 0;
uint32_t lastDisplayAt = 0;

const char* airQualityString(AirQuality q) {
  switch (q) {
    case AirQuality::GOOD: return "GOOD";
    case AirQuality::MODERATE: return "MODERATE";
    case AirQuality::POOR: return "POOR";
  }
  return "GOOD";
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

// Local rule only — never depends on Cloud, matches PRD "khong phu thuoc
// Internet de hoat dong co ban". CO2 is the required core (PRD 4.1); PM2.5
// only factors in when the optional module is actually present — whichever
// pollutant is worse wins when both are available.
AirQuality computeAirQuality(uint16_t co2Ppm, float pm25Ugm3, bool pm25Valid) {
  if (co2Ppm >= CO2_POOR_PPM || (pm25Valid && pm25Ugm3 >= PM25_POOR_UGM3)) return AirQuality::POOR;
  if (co2Ppm >= CO2_MODERATE_PPM || (pm25Valid && pm25Ugm3 >= PM25_MODERATE_UGM3)) return AirQuality::MODERATE;
  return AirQuality::GOOD;
}

void printStatus() {
  Serial.print("[APP] CO2=");
  Serial.print(co2);
  Serial.print("ppm PM2.5=");
  if (pm25Ready) Serial.print(pm25); else Serial.print("n/a");
  Serial.print(" T=");
  Serial.print(temperature);
  Serial.print("C H=");
  Serial.print(humidity);
  Serial.print("% Status=");
  Serial.println(airQualityString(airQuality));
}

void readSensorsAndUpdate() {
  if (scd4xReady && scd4x.readMeasurement()) {
    co2 = scd4x.getCO2();
    temperature = scd4x.getTemperature();
    humidity = scd4x.getHumidity();
  }

  if (pm25Ready) {
    PM25_AQI_Data pmData;
    if (pm25Sensor.read(&pmData)) {
      pm25 = pmData.pm25_env;
    }
  }

  airQuality = computeAirQuality(co2, pm25, pm25Ready);

  if (co2 > 0) iot.report(CONTROL_CO2, static_cast<int>(co2));
  if (pm25Ready) iot.report(CONTROL_PM25, static_cast<int>(pm25));
  if (!isnan(temperature)) iot.report(CONTROL_TEMPERATURE, temperature);
  if (!isnan(humidity)) iot.report(CONTROL_HUMIDITY, humidity);
  iot.report(CONTROL_AIR_STATUS, airQualityString(airQuality));

  printStatus();
}

void updateDisplay() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("CO2:   ");
  display.print(co2);
  display.println(" ppm");

  display.setCursor(0, 12);
  display.print("PM2.5: ");
  if (pm25Ready) display.print(pm25); else display.print("--");
  display.println(" ug/m3");

  display.setCursor(0, 24);
  display.print("Temp:  ");
  if (isnan(temperature)) display.print("--"); else display.print(temperature, 1);
  display.println(" C");

  display.setCursor(0, 36);
  display.print("Humi:  ");
  if (isnan(humidity)) display.print("--"); else display.print(humidity, 0);
  display.println(" %");

  display.setCursor(0, 50);
  if (!iot.provisioned()) {
    display.print("Setup: ");
    display.println(iot.provisioningApName());
  } else {
    display.print(airQualityString(airQuality));
    display.print(muted ? " (muted)" : "");
    display.print(" Cloud:");
    display.println(iot.connected() ? "ON" : "OFF");
  }

  display.display();
}

uint32_t colorForAirQuality(AirQuality q) {
  switch (q) {
    case AirQuality::GOOD: return statusLed.Color(0, 40, 0);
    case AirQuality::MODERATE: return statusLed.Color(40, 25, 0);
    case AirQuality::POOR: return statusLed.Color(40, 0, 0);
  }
  return statusLed.Color(0, 0, 0);
}

void updateAlert() {
  if (muted && static_cast<int32_t>(millis() - mutedUntil) >= 0) {
    muted = false;
  }

  statusLed.setPixelColor(0, colorForAirQuality(airQuality));
  statusLed.show();

  const bool shouldSound = buzzerEnabled && !muted && airQuality == AirQuality::POOR;
  digitalWrite(BUZZER_PIN, shouldSound ? HIGH : LOW);
}

void muteAlert() {
  muted = true;
  mutedUntil = millis() + MUTE_DURATION_MS;

  JsonDocument value;
  value["state"] = "pressed";
  iot.report(CONTROL_MUTE_BUTTON, value.as<JsonVariantConst>());
}

void updateButton() {
  const bool raw = digitalRead(MUTE_BUTTON_PIN);
  const uint32_t now = millis();

  if (raw != lastButtonRaw) {
    lastButtonRaw = raw;
    lastButtonChangeAt = now;
  }
  if (now - lastButtonChangeAt < BUTTON_DEBOUNCE_MS || raw == buttonStable) return;

  buttonStable = raw;
  if (buttonStable == LOW) muteAlert();
}

// Long-press BOOT (10s) -> factory reset, same pattern as example
// 05_Advanced_SmartWatering's updateBootButton().
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

void onBuzzerEnableControl(bool value) {
  buzzerEnabled = value;
  iot.report(CONTROL_BUZZER_EN, buzzerEnabled);
}

// controlTypeButton command handler — Console/Mobile "Mute Alert" tap, wire
// payload {"state":"pressed"} per iotlabs-sdk Control Type Registry section
// 12 "Button". Same mute path as the physical button (muteAlert()).
void onMuteButtonControl(const IoTLabsValue& value) {
  if (!value.has("state")) return;
  if (value["state"].asString() == "pressed") muteAlert();
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

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(MUTE_BUTTON_PIN, INPUT_PULLUP);
  lastButtonRaw = digitalRead(MUTE_BUTTON_PIN);
  buttonStable = lastButtonRaw;

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  lastBootButtonRaw = digitalRead(BOOT_BUTTON_PIN);

  Wire.begin(I2C_SDA, I2C_SCL);

  scd4xReady = scd4x.begin(Wire);  // measBegin defaults true: starts periodic measurement
  if (!scd4xReady) Serial.println("[APP] SCD4x init failed");

  // Optional module (PRD 4.1) — CO2-only is a fully valid V1 build if this
  // fails to init; only affects control_2_value reporting/display.
  pmSerial.begin(9600, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);
  pm25Ready = pm25Sensor.begin_UART(&pmSerial);
  if (!pm25Ready) Serial.println("[APP] PM2.5 sensor not detected — running CO2-only (optional module)");

  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledReady) Serial.println("[APP] OLED init failed");

  statusLed.begin();
  statusLed.show();

  iot.report(CONTROL_BUZZER_EN, buzzerEnabled, false);

  iot.on(CONTROL_BUZZER_EN, onBuzzerEnableControl);
  iot.on(CONTROL_MUTE_BUTTON, onMuteButtonControl);
  iot.onEvent(onIoTLabsEvent);

  iot.begin();

  readSensorsAndUpdate();
  lastSensorAt = millis();
}

void loop() {
  iot.loop();

  // Local hardware remains responsive even when Cloud is offline.
  updateButton();
  updateBootButton();
  updateAlert();

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
