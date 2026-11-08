/*
  IoTLabs 08 — Multi Devices Control — AirQuality2 (phòng khách)

  ONE of 5 independent devices in this project (see
  IoTLabs internal design notes for the full PRD).
  Independent from the other 4 devices — no gateway, no shared state. Same
  sensor (SCD4x) and control set as AirQuality1 (bedroom) — both devices
  share ONE Device Profile ("Multi Devices — AirQuality", see
  02_PRD_CONTROLS_VA_AUTOMATION.md §3); only the derived text-control
  semantics differ per room, hence two separate sketches instead of a
  runtime "role" branch (same "no role branching" principle as example 07).

  Additional Arduino Library Manager dependency:
    SparkFun SCD4x Arduino Library (CO2 + temperature + humidity, I2C)

  Controls (see 02_PRD_CONTROLS_VA_AUTOMATION.md §2.5):
    control_1_value  CO2 (ppm, gauge)
    control_2_value  temperature (C)
    control_3_value  humidity (%, gauge)
    control_4_text   air_quality_level (GOOD / MODERATE / POOR)
    control_5_text   occupancy_air_change ("stable" / "rising_fast") —
                      living-room-specific: a fast CO2 rise between two
                      sensor reads suggests more people just entered the room
    control_6_text   ventilation_required ("yes" / "no")

  Local rule only, no cloud dependency (matches example 06's PRD
  non-functional requirement): CO2 thresholds below are the same reference
  values already used by example 06, not yet field-validated on real
  hardware — re-tune after real-world testing.

  Cross-device automation (this device's CO2 alert auto-turning on another
  device's fan, e.g. SmartPlug) is explicitly OUT of scope for this example
  — see 02_PRD_CONTROLS_VA_AUTOMATION.md §5.1. The alert here is purely
  informational on the shared Dashboard; the user reacts manually.

  GPIO (ESP32-C3 Supermini):
    SCD4x SDA        GPIO4
    SCD4x SCL        GPIO5
    LED RGB WS2812   GPIO8   (onboard status LED — verify against real board)
    BOOT button      GPIO9   hold >=10s = factory reset

  Provisioning page: built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport), no custom provisioning_page.h needed.
*/

#define IOTLABS_PROJECT_NAME "AirQuality2 Setup"
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
#include <Adafruit_NeoPixel.h>

// ---- GPIO --------------------------------------------------------------
constexpr uint8_t I2C_SDA = 4;
constexpr uint8_t I2C_SCL = 5;
constexpr uint8_t STATUS_LED_PIN = 8;
constexpr uint8_t BOOT_BUTTON_PIN = 9;

// ---- Control IDs — match PROFILE_INTENT.json ----------------------------
constexpr char CONTROL_CO2[] = "control_1_value";
constexpr char CONTROL_TEMPERATURE[] = "control_2_value";
constexpr char CONTROL_HUMIDITY[] = "control_3_value";
constexpr char CONTROL_AIR_QUALITY[] = "control_4_text";
constexpr char CONTROL_OCCUPANCY[] = "control_5_text";
constexpr char CONTROL_VENTILATION[] = "control_6_text";

// SCD4x's own periodic-measurement update interval is 5s — same as example 06.
constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 50;

// Reference thresholds, same as example 06 — not yet field-validated.
constexpr uint16_t CO2_MODERATE_PPM = 1000;
constexpr uint16_t CO2_POOR_PPM = 2000;
// A CO2 jump of this much between two consecutive 5s reads suggests more
// people just entered the (living) room — reference value, not field-tuned.
constexpr uint16_t CO2_FAST_RISE_DELTA_PPM = 150;

enum class AirQuality { GOOD, MODERATE, POOR };

IoTLabs iot;
SCD4x scd4x;
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

bool scd4xReady = false;
uint16_t co2 = 0;
uint16_t previousCo2 = 0;
bool hasPreviousCo2 = false;
float temperature = NAN;
float humidity = NAN;
AirQuality airQuality = AirQuality::GOOD;

bool bootButtonLastRaw = HIGH;
uint32_t bootButtonDownAt = 0;
bool factoryResetTriggered = false;
uint32_t lastSensorAt = 0;
uint32_t lastLedUpdateAt = 0;

struct LedColor { uint8_t r, g, b; };
constexpr LedColor LED_OFF_COLOR = {0, 0, 0};
constexpr LedColor LED_BLUE = {0, 0, 40};
constexpr LedColor LED_YELLOW = {40, 40, 0};
constexpr LedColor LED_GREEN = {0, 20, 0};
constexpr LedColor LED_RED = {40, 0, 0};

const char* airQualityString(AirQuality q) {
  switch (q) {
    case AirQuality::GOOD: return "GOOD";
    case AirQuality::MODERATE: return "MODERATE";
    case AirQuality::POOR: return "POOR";
  }
  return "GOOD";
}

AirQuality computeAirQuality(uint16_t co2Ppm) {
  if (co2Ppm >= CO2_POOR_PPM) return AirQuality::POOR;
  if (co2Ppm >= CO2_MODERATE_PPM) return AirQuality::MODERATE;
  return AirQuality::GOOD;
}

void setStatusLed(const LedColor& c) {
  statusLed.setPixelColor(0, statusLed.Color(c.r, c.g, c.b));
  statusLed.show();
}

// Same LED priority state machine as other devices in this project.
void updateStatusLed() {
  const uint32_t now = millis();
  if (now - lastLedUpdateAt < LED_UPDATE_INTERVAL_MS) return;
  lastLedUpdateAt = now;

  const IoTLabsStatus status = iot.status();
  LedColor base;
  bool blink = false;

  if (status == IoTLabsStatus::OFFLINE || status == IoTLabsStatus::ERROR) {
    base = LED_RED; blink = true;
  } else if (status == IoTLabsStatus::UNPROVISIONED || status == IoTLabsStatus::PROVISIONING) {
    base = LED_YELLOW; blink = true;
  } else if (status == IoTLabsStatus::BOOT || status == IoTLabsStatus::WIFI_CONNECTING ||
             status == IoTLabsStatus::ACTIVATING || status == IoTLabsStatus::CLOUD_CONNECTING) {
    base = LED_BLUE; blink = true;
  } else {
    base = LED_GREEN; blink = false;
  }

  if (blink) {
    setStatusLed(((now / 500) % 2 == 0) ? base : LED_OFF_COLOR);
  } else {
    setStatusLed(base);
  }
}

void readAndReportAirQuality() {
  if (!scd4xReady || !scd4x.readMeasurement()) return;

  co2 = scd4x.getCO2();
  temperature = scd4x.getTemperature();
  humidity = scd4x.getHumidity();
  airQuality = computeAirQuality(co2);

  const bool risingFast = hasPreviousCo2 && (co2 > previousCo2) &&
                           (co2 - previousCo2 >= CO2_FAST_RISE_DELTA_PPM);
  previousCo2 = co2;
  hasPreviousCo2 = true;

  iot.report(CONTROL_CO2, static_cast<int>(co2));
  iot.report(CONTROL_TEMPERATURE, temperature);
  iot.report(CONTROL_HUMIDITY, humidity);
  iot.report(CONTROL_AIR_QUALITY, airQualityString(airQuality));
  iot.report(CONTROL_OCCUPANCY, risingFast ? "rising_fast" : "stable");
  iot.report(CONTROL_VENTILATION, (airQuality == AirQuality::POOR) ? "yes" : "no");
}

// Hold >=10s = factory reset, same pattern as other devices in this project.
void updateBootButton() {
  const bool raw = digitalRead(BOOT_BUTTON_PIN);
  const uint32_t now = millis();

  if (raw == LOW && bootButtonLastRaw == HIGH) {
    bootButtonDownAt = now;
    factoryResetTriggered = false;
  }

  if (raw == LOW && !factoryResetTriggered && (now - bootButtonDownAt) >= FACTORY_RESET_HOLD_MS) {
    factoryResetTriggered = true;
    iot.factoryReset();
  }

  bootButtonLastRaw = raw;
}

void setup() {
  Serial.begin(115200);

  statusLed.begin();
  setStatusLed(LED_OFF_COLOR);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  bootButtonLastRaw = digitalRead(BOOT_BUTTON_PIN);

  Wire.begin(I2C_SDA, I2C_SCL);
  scd4xReady = scd4x.begin(Wire);  // measBegin defaults true: starts periodic measurement
  if (!scd4xReady) Serial.println("[APP] SCD4x init failed");

  iot.begin();

  readAndReportAirQuality();
  lastSensorAt = millis();
}

void loop() {
  iot.loop();
  updateStatusLed();
  updateBootButton();

  const uint32_t now = millis();
  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
    lastSensorAt = now;
    readAndReportAirQuality();
  }
}
