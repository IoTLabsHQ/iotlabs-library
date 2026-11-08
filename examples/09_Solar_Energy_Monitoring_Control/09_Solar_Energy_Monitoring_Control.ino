/*
  IoTLabs Advanced 09 — Solar Energy Monitoring & Control

  Source concept: https://maker.iotlabs.vn/projects/he-thong-giam-sat-va-dieu-khien-nang-luong-mat-troi-voi-esp32

  ============================================================================
  SAFETY SCOPE — LOW-VOLTAGE DC ONLY (READ BEFORE WIRING ANYTHING)
  ============================================================================
  This example targets a small, low-voltage DC solar setup: PV panel -> INA219
  -> battery/charge path -> DC load, in the 6-12V class. It is explicitly
  OUT OF SCOPE for:
    - 220V/mains AC of any kind
    - Inverters or grid-tie equipment
    - Any DC string/panel wiring exceeding 50V
  Do not adapt this firmware/wiring to a higher-voltage or AC system without
  a full re-design by someone qualified in that domain. See
  IoTLabs internal design notes
  for the full scope statement.
  ============================================================================

  Additional Arduino Library Manager dependency:
    Adafruit INA219 (by Adafruit) — pulls in Adafruit BusIO as a transitive
    dependency automatically via Arduino Library Manager.

  Board: classic ESP32 DevKit (NOT the ESP32-C3 Supermini used by examples
  07/08) — FQBN esp32:esp32:esp32. No CDCOnBoot quirk applies to this board.

  GPIO:
    INA219 (I2C, PV voltage/current/power)  SDA=GPIO21  SCL=GPIO22  (ESP32
                                             default Wire pins, address 0x40)
    Battery voltage divider (ADC1, input-only, safe — avoids ADC2/WiFi
    conflict)                               GPIO34
    DC load relay/MOSFET control (digital output)  GPIO25

  Battery voltage divider: BATTERY_DIVIDER_RATIO below is a PLACEHOLDER and
  MUST be calibrated against your actual resistor pair using a real
  multimeter reading before this number can be trusted — see
  01_SPEC_KIEN_TRUC_HE_THONG.md §"Calibration procedure" for the exact steps.
  Never assume a specific resistor pair silently.

  Controls (see PROFILE_INTENT.json):
    control_1_value   PV voltage   (V)  read
    control_2_value   PV current   (mA) read
    control_3_value   PV power     (mW) read
    control_4_value   Battery voltage (V, gauge min/max 0-15) read
    control_5_on_off  DC load      read_write
    control_6_select  Operating mode ("auto"/"manual") read_write

  AUTO/MANUAL behavior (see 02_PRD_CONTROLS_VA_AUTOMATION.md for full state
  machine):
    - AUTO (default): classic low-voltage-disconnect/reconnect hysteresis.
      battery_voltage <= BATTERY_LVD_VOLTS -> force load OFF.
      battery_voltage >= BATTERY_LVR_VOLTS -> allow load back ON.
      Between the two thresholds, the load holds whatever state it already
      has (prevents relay chatter right at one threshold) — same hysteresis
      pattern as example 05_Advanced_SmartWatering's soil thresholds.
    - MANUAL: load is fully driven by Console/Mobile writes to control_5;
      AUTO thresholds are not applied while in manual. A write to
      control_5_on_off while in AUTO mode is ignored (AUTO owns the relay)
      and the device reports back its current real state instead, so the
      UI never shows a stale/optimistic value.
    - BATTERY_LVD_VOLTS/BATTERY_LVR_VOLTS are #define-level constants that
      MUST be tuned to your actual battery chemistry/voltage class (a 6V,
      9V, and 12V lead-acid/LiFePO4 system all need different values) —
      the defaults below are a generic 12V lead-acid placeholder only.

  Provisioning page: built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport), no custom provisioning_page.h needed.
*/

#define IOTLABS_PROJECT_NAME "Solar Energy Monitor Setup"
#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// ---- GPIO ------------------------------------------------------------------
constexpr uint8_t I2C_SDA = 21;
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t BATTERY_ADC_PIN = 34;   // ADC1, input-only — safe with WiFi active
constexpr uint8_t LOAD_RELAY_PIN = 25;

// ---- Control IDs — match PROFILE_INTENT.json (1-indexed, control_N_typeKey,
// same auto-generated-ID convention used by every prior example) ------------
constexpr char CONTROL_PV_VOLTAGE[] = "control_1_value";
constexpr char CONTROL_PV_CURRENT[] = "control_2_value";
constexpr char CONTROL_PV_POWER[] = "control_3_value";
constexpr char CONTROL_BATTERY_VOLTAGE[] = "control_4_value";
constexpr char CONTROL_LOAD[] = "control_5_on_off";
constexpr char CONTROL_MODE[] = "control_6_select";

constexpr uint32_t SENSOR_INTERVAL_MS = 2000;

// ---- Battery voltage divider — CALIBRATE with your actual resistor values --
// ADC reads V_ADC = V_BATTERY / BATTERY_DIVIDER_RATIO through a resistor
// divider (R_top from BATTERY+ to GPIO34, R_bottom from GPIO34 to GND).
// BATTERY_DIVIDER_RATIO = (R_top + R_bottom) / R_bottom.
// The value below (ratio 6.0, e.g. R_top=100k / R_bottom=20k) is a
// PLACEHOLDER for a ~0-15V battery class only — measure your real battery
// voltage with a multimeter, compare against the raw ADC voltage this
// firmware computes, and replace this constant with the true ratio of the
// resistors you actually installed before trusting any reported value.
constexpr float BATTERY_DIVIDER_RATIO = 6.0f;  // CALIBRATE with your actual resistor values
constexpr float ADC_REF_VOLTAGE = 3.3f;
constexpr uint16_t ADC_MAX_VALUE = 4095;  // 12-bit (analogReadResolution(12))
constexpr int ADC_SAMPLE_COUNT = 16;

// ---- Low-voltage disconnect/reconnect hysteresis — TUNE to your battery
// chemistry/voltage class (6V/9V/12V system all need different numbers).
// LVR MUST stay greater than LVD to avoid relay chatter right at one
// threshold — generic 12V lead-acid placeholder shown below.
constexpr float BATTERY_LVD_VOLTS = 11.0f;  // TUNE: load-off threshold
constexpr float BATTERY_LVR_VOLTS = 12.4f;  // TUNE: load-on threshold (must be > LVD)

enum class OperatingMode { AUTO, MANUAL };

IoTLabs iot;
Adafruit_INA219 ina219;

bool inaReady = false;
OperatingMode mode = OperatingMode::AUTO;
bool loadOn = false;
float lastBatteryVoltage = 0.0f;
uint32_t lastSensorAt = 0;

const char* modeString(OperatingMode m) {
  return (m == OperatingMode::AUTO) ? "auto" : "manual";
}

void applyLoad(bool on) {
  loadOn = on;
  digitalWrite(LOAD_RELAY_PIN, loadOn ? HIGH : LOW);
}

void reportLoad(bool publishNow = true) {
  iot.report(CONTROL_LOAD, loadOn, publishNow);
}

void reportMode(bool publishNow = true) {
  iot.report(CONTROL_MODE, modeString(mode), publishNow);
}

// AUTO hysteresis (see file header): battery <= LVD -> force OFF;
// battery >= LVR -> allow back ON; the band between the two thresholds
// holds whatever state the load already has (prevents relay chattering
// right at one threshold) — same pattern as example 05's soil hysteresis.
void applyAutoControl() {
  if (mode != OperatingMode::AUTO) return;

  if (lastBatteryVoltage <= BATTERY_LVD_VOLTS && loadOn) {
    applyLoad(false);
    reportLoad();
  } else if (lastBatteryVoltage >= BATTERY_LVR_VOLTS && !loadOn) {
    applyLoad(true);
    reportLoad();
  }
}

// Console/Mobile writes control_5_on_off. AUTO mode owns the relay while
// active — a write received during AUTO is ignored and the device reports
// its current real state back instead, so the UI never shows a stale
// optimistic value. In MANUAL, the write is applied immediately.
void onLoadControl(bool on) {
  if (mode == OperatingMode::AUTO) {
    reportLoad();  // AUTO owns the relay — echo real state, do not apply
    return;
  }
  applyLoad(on);
  reportLoad();
}

// Console/Mobile writes control_6_select ("auto"/"manual"). Switching back
// to AUTO immediately re-evaluates the hysteresis against the last known
// battery reading (does not wait for the next sensor cycle). Switching to
// MANUAL leaves the load exactly as it is until the user explicitly writes
// control_5_on_off.
void onModeControl(const String& value) {
  if (value == "auto") {
    mode = OperatingMode::AUTO;
    reportMode();
    applyAutoControl();
  } else if (value == "manual") {
    mode = OperatingMode::MANUAL;
    reportMode();
  } else {
    reportMode();  // reject unknown value, echo current mode
  }
}

int readBatteryAdcRawAverage() {
  long total = 0;
  for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
    total += analogRead(BATTERY_ADC_PIN);
    delay(2);
  }
  return static_cast<int>(total / ADC_SAMPLE_COUNT);
}

float readBatteryVoltage() {
  const int raw = readBatteryAdcRawAverage();
  const float adcVoltage = (static_cast<float>(raw) / static_cast<float>(ADC_MAX_VALUE)) * ADC_REF_VOLTAGE;
  return adcVoltage * BATTERY_DIVIDER_RATIO;
}

void printStatus(float pvVoltage, float pvCurrent, float pvPower) {
  Serial.print("[APP] PV=");
  Serial.print(pvVoltage);
  Serial.print("V ");
  Serial.print(pvCurrent);
  Serial.print("mA ");
  Serial.print(pvPower);
  Serial.print("mW Battery=");
  Serial.print(lastBatteryVoltage);
  Serial.print("V Load=");
  Serial.print(loadOn ? "ON" : "OFF");
  Serial.print(" Mode=");
  Serial.println(modeString(mode));
}

void readSensorsAndReport() {
  float pvVoltage = 0.0f;
  float pvCurrent = 0.0f;
  float pvPower = 0.0f;

  if (inaReady) {
    pvVoltage = ina219.getBusVoltage_V();
    pvCurrent = ina219.getCurrent_mA();
    pvPower = ina219.getPower_mW();
    iot.report(CONTROL_PV_VOLTAGE, pvVoltage);
    iot.report(CONTROL_PV_CURRENT, pvCurrent);
    iot.report(CONTROL_PV_POWER, pvPower);
  }

  lastBatteryVoltage = readBatteryVoltage();
  iot.report(CONTROL_BATTERY_VOLTAGE, lastBatteryVoltage);

  applyAutoControl();

  printStatus(pvVoltage, pvCurrent, pvPower);
}

void onIoTLabsEvent(IoTLabsEvent event) {
  if (event == IoTLabsEvent::PROVISIONING_STARTED) {
    Serial.print("[APP] Provisioning — connect to: ");
    Serial.println(iot.provisioningApName());
  } else if (event == IoTLabsEvent::ONLINE) {
    Serial.println("[APP] ONLINE");
  } else if (event == IoTLabsEvent::ERROR) {
    Serial.print("[APP] Error: ");
    Serial.println(iot.lastError());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LOAD_RELAY_PIN, OUTPUT);
  digitalWrite(LOAD_RELAY_PIN, LOW);  // safe boot: load OFF until AUTO/MANUAL decides

  analogReadResolution(12);

  Wire.begin(I2C_SDA, I2C_SCL);
  inaReady = ina219.begin(&Wire);
  if (!inaReady) {
    Serial.println("[APP] INA219 init failed — pv_voltage/pv_current/pv_power will not be reported");
  }

  reportLoad(false);
  reportMode(false);

  iot.on(CONTROL_LOAD, onLoadControl);
  iot.on(CONTROL_MODE, onModeControl);
  iot.onEvent(onIoTLabsEvent);

  iot.begin();

  readSensorsAndReport();
  lastSensorAt = millis();
}

void loop() {
  iot.loop();

  const uint32_t now = millis();
  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
    lastSensorAt = now;
    readSensorsAndReport();
  }
}
