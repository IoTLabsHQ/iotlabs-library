/*
  IoTLabs 08 — Multi Devices Control — DoorSecurity

  ONE of 5 independent devices in this project (see
  IoTLabs internal design notes for the full PRD).
  Independent from the other 4 devices — no gateway, no shared state.

  Controls (see 02_PRD_CONTROLS_VA_AUTOMATION.md §2.3):
    control_1_text    door_state ("open" / "closed"), read-only
    control_2_select  security_mode ("disarmed" / "home" / "away"), read_write
    control_3_on_off  alarm (buzzer), read_write — auto-set true on intrusion,
                      user can also toggle manually from Dashboard
    control_4_value   open_duration (seconds while currently open), read
    control_5_value   open_count (lifetime count), read
    control_6_text    intrusion event, read

  Battery level (control_7_value) is an OPTIONAL hardware extension — this
  example runs on USB/mains power, not battery, so it is not implemented
  here. Add an ADC read + iot.report() pair per PRD §2.3 if a battery-powered
  build is made later.

  Intrusion rule (local, no cloud dependency): the door opening while
  security_mode == "away" is treated as an intrusion — reports control_6_text
  and auto-sets the alarm on. Opening the door in "home" or "disarmed" mode
  is normal, no intrusion event.

  GPIO (ESP32-C3 Supermini):
    Door sensor (reed switch, NC) GPIO4   INPUT_PULLUP — LOW = closed (magnet
                                           present), HIGH = open
    Buzzer/alarm output           GPIO5   active HIGH
    BOOT button                   GPIO9   hold >=10s = factory reset (same
                                           pattern as example 07; no short-
                                           press action on this device)
    LED RGB WS2812                GPIO8   onboard status LED — verify against
                                           real board

  Provisioning page: built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport), no custom provisioning_page.h needed.
*/

#define IOTLABS_PROJECT_NAME "DoorSecurity Setup"
#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <Adafruit_NeoPixel.h>

// ---- GPIO --------------------------------------------------------------
constexpr uint8_t DOOR_SENSOR_PIN = 4;
constexpr uint8_t BUZZER_PIN = 5;
constexpr uint8_t STATUS_LED_PIN = 8;
constexpr uint8_t BOOT_BUTTON_PIN = 9;

// ---- Control IDs — match PROFILE_INTENT.json ----------------------------
constexpr char CONTROL_DOOR_STATE[] = "control_1_text";
constexpr char CONTROL_SECURITY_MODE[] = "control_2_select";
constexpr char CONTROL_ALARM[] = "control_3_on_off";
constexpr char CONTROL_OPEN_DURATION[] = "control_4_value";
constexpr char CONTROL_OPEN_COUNT[] = "control_5_value";
constexpr char CONTROL_INTRUSION[] = "control_6_text";

constexpr uint32_t DOOR_DEBOUNCE_MS = 100;
constexpr uint32_t OPEN_DURATION_REPORT_INTERVAL_MS = 1000;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 50;

constexpr char MODE_DISARMED[] = "disarmed";
constexpr char MODE_HOME[] = "home";
constexpr char MODE_AWAY[] = "away";

IoTLabs iot;
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

bool doorOpen = false;
bool doorLastRaw = false;
bool doorStable = false;
uint32_t doorLastChangeAt = 0;
uint32_t doorOpenedAt = 0;
uint32_t openCount = 0;
uint32_t lastOpenDurationReportAt = 0;

char securityMode[16] = "disarmed";
bool alarmOn = false;

bool bootButtonLastRaw = HIGH;
uint32_t bootButtonDownAt = 0;
bool factoryResetTriggered = false;
uint32_t lastLedUpdateAt = 0;

struct LedColor { uint8_t r, g, b; };
constexpr LedColor LED_OFF_COLOR = {0, 0, 0};
constexpr LedColor LED_BLUE = {0, 0, 40};
constexpr LedColor LED_YELLOW = {40, 40, 0};
constexpr LedColor LED_GREEN = {0, 20, 0};
constexpr LedColor LED_RED = {40, 0, 0};

void setStatusLed(const LedColor& c) {
  statusLed.setPixelColor(0, statusLed.Color(c.r, c.g, c.b));
  statusLed.show();
}

// Same LED priority state machine as other devices in this project — plus
// alarm overlay (fast red blink) takes priority over everything when firing.
void updateStatusLed() {
  const uint32_t now = millis();
  if (now - lastLedUpdateAt < LED_UPDATE_INTERVAL_MS) return;
  lastLedUpdateAt = now;

  if (alarmOn) {
    setStatusLed(((now / 150) % 2 == 0) ? LED_RED : LED_OFF_COLOR);
    return;
  }

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

void applyAlarm(bool on) {
  alarmOn = on;
  digitalWrite(BUZZER_PIN, alarmOn ? HIGH : LOW);
}

// Console/Mobile writes alarm on/off -> apply, report back.
void onAlarmControl(bool on) {
  applyAlarm(on);
  iot.report(CONTROL_ALARM, alarmOn);
}

// Console/Mobile writes security mode -> validate against known modes,
// apply, report back.
void onSecurityModeControl(const String& value) {
  if (value != MODE_DISARMED && value != MODE_HOME && value != MODE_AWAY) return;
  value.toCharArray(securityMode, sizeof(securityMode));
  iot.report(CONTROL_SECURITY_MODE, securityMode);
}

void reportDoorState() {
  iot.report(CONTROL_DOOR_STATE, doorOpen ? "open" : "closed");
}

// Local rule, no cloud dependency (see file header): opening the door while
// armed "away" is an intrusion — auto-triggers the alarm and reports the
// event. "home"/"disarmed" openings are normal, no event.
void handleDoorOpened() {
  openCount++;
  doorOpenedAt = millis();
  reportDoorState();
  iot.report(CONTROL_OPEN_COUNT, static_cast<long>(openCount));
  iot.report(CONTROL_OPEN_DURATION, 0);

  if (strcmp(securityMode, MODE_AWAY) == 0) {
    iot.report(CONTROL_INTRUSION, "intrusion_detected");
    applyAlarm(true);
    iot.report(CONTROL_ALARM, alarmOn);
  }
}

void handleDoorClosed() {
  reportDoorState();
  iot.report(CONTROL_OPEN_DURATION, 0);
}

// Debounced digital read of the reed switch; on a stable open->closed or
// closed->open transition, fires the corresponding handler above.
void updateDoorSensor() {
  const bool raw = (digitalRead(DOOR_SENSOR_PIN) == HIGH);  // HIGH = open
  const uint32_t now = millis();

  if (raw != doorLastRaw) {
    doorLastRaw = raw;
    doorLastChangeAt = now;
  }

  if (now - doorLastChangeAt >= DOOR_DEBOUNCE_MS && doorStable != raw) {
    doorStable = raw;
    doorOpen = doorStable;
    if (doorOpen) {
      handleDoorOpened();
    } else {
      handleDoorClosed();
    }
  }

  if (doorOpen && now - lastOpenDurationReportAt >= OPEN_DURATION_REPORT_INTERVAL_MS) {
    lastOpenDurationReportAt = now;
    const uint32_t seconds = (now - doorOpenedAt) / 1000;
    iot.report(CONTROL_OPEN_DURATION, static_cast<long>(seconds));
  }
}

// Hold >=10s = factory reset (no short-press action on this device).
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

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  bootButtonLastRaw = digitalRead(BOOT_BUTTON_PIN);

  doorLastRaw = (digitalRead(DOOR_SENSOR_PIN) == HIGH);
  doorStable = doorLastRaw;
  doorOpen = doorStable;

  reportDoorState();
  iot.report(CONTROL_SECURITY_MODE, securityMode, false);
  iot.report(CONTROL_ALARM, alarmOn, false);
  iot.report(CONTROL_OPEN_DURATION, 0, false);
  iot.report(CONTROL_OPEN_COUNT, static_cast<long>(openCount), false);

  iot.on(CONTROL_ALARM, onAlarmControl);
  iot.on(CONTROL_SECURITY_MODE, onSecurityModeControl);
  iot.begin();
}

void loop() {
  iot.loop();
  updateStatusLed();
  updateDoorSensor();
  updateBootButton();
}
