/*
  IoTLabs Advanced 07 — Smart Home Complete (AutoPinMapping variant)

  Same device behavior as examples/07_Smart_Home_Complete, with
  ONLY the relay rewritten to iot.setPinMapping(). The boot button stays
  hand-wired on purpose: a short press both toggles the relay locally AND
  reports a "pressed" event, and a >=10s hold triggers a factory reset --
  setPinMapping()'s DIGITAL_IN_DEBOUNCED mode only auto-reports (a bool
  state, or one fixed string on release), it has no hook for "and also
  toggle a different control" or hold-duration logic, so forcing this
  button through it would silently drop the toggle/factory-reset behavior.
  DHT/LED are unaffected either way (I2C/1-wire-library and NeoPixel-driven,
  not raw-pin).

  Since the relay's on/off state now lives behind setPinMapping() instead of
  a sketch-level `relayOn` bool, anything that used to read that variable
  (the LED white-blink overlay, the button's toggle target) reads the
  physical pin directly via isRelayOn() instead -- same source of truth the
  original variable was shadowing anyway.

  Additional Arduino Library Manager dependencies:
    DHT sensor library by Adafruit
    Adafruit NeoPixel

  GPIO (fixed/hardcoded, identical on every board):
    Relay control   GPIO4
    DHT11 data      GPIO5
    LED RGB WS2812  GPIO8   (onboard — verify against the actual board)
    BOOT button     GPIO9   (short press = toggle relay, hold >=10s =
                             factory reset)
    UART0 RX/TX     GPIO20/21 (USB-Serial, unused by app code)

  Provisioning page: built-in IoTLabs Library SoftAP setup page
  (Esp32SoftApProvisioningTransport), same as the original.
*/

#define IOTLABS_PROJECT_NAME "Smart Home Complete Setup"
#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_DEVICE_ACTIVATION_SECRET "paste-the-one-time-activation-token-from-Console-here"  // optional: skip retyping the token into the SoftAP portal while iterating
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

// ---- GPIO (fixed, identical on every board) --------------------------------
constexpr uint8_t RELAY_PIN = 4;
constexpr uint8_t DHT_PIN = 5;
constexpr uint8_t LED_PIN = 8;
constexpr uint8_t BOOT_BUTTON_PIN = 9;

// ---- Control IDs — match PROFILE_INTENT.json -------------------------------
constexpr char CONTROL_ON_OFF[] = "control_1_on_off";
constexpr char CONTROL_BUTTON[] = "control_2_button";
constexpr char CONTROL_TEMPERATURE[] = "control_3_value";
constexpr char CONTROL_HUMIDITY[] = "control_4_value";

constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
constexpr uint8_t DHT_ERROR_THRESHOLD = 5;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 50;

IoTLabs iot;
DHT dht(DHT_PIN, DHT11);
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---- Button state (relay state now lives behind setPinMapping(), read via
// isRelayOn() instead of a shadow variable) -----------------------------------
bool buttonLastRaw = HIGH;
bool buttonStable = HIGH;
uint32_t buttonLastChangeAt = 0;
uint32_t buttonPressStartAt = 0;
bool factoryResetTriggered = false;

// ---- Climate sensing state ---------------------------------------------------
uint32_t lastSensorAt = 0;
uint8_t consecutiveDhtErrors = 0;
bool sensorError = false;

// ---- LED RGB state machine (PRD §5) ------------------------------------------
uint32_t lastLedUpdateAt = 0;

struct LedColor { uint8_t r, g, b; };
constexpr LedColor LED_OFF_COLOR = {0, 0, 0};
constexpr LedColor LED_BLUE = {0, 0, 40};
constexpr LedColor LED_YELLOW = {40, 40, 0};
constexpr LedColor LED_GREEN = {0, 20, 0};
constexpr LedColor LED_RED = {40, 0, 0};
constexpr LedColor LED_WHITE = {30, 30, 30};

void setLed(const LedColor& c) {
  led.setPixelColor(0, led.Color(c.r, c.g, c.b));
  led.show();
}

void updateLed() {
  const uint32_t now = millis();
  if (now - lastLedUpdateAt < LED_UPDATE_INTERVAL_MS) return;
  lastLedUpdateAt = now;

  const IoTLabsStatus status = iot.status();
  LedColor base;
  bool blink = false;

  if (sensorError || status == IoTLabsStatus::OFFLINE || status == IoTLabsStatus::ERROR) {
    base = LED_RED; blink = true;
  } else if (status == IoTLabsStatus::UNPROVISIONED || status == IoTLabsStatus::PROVISIONING) {
    base = LED_YELLOW; blink = true;
  } else if (status == IoTLabsStatus::BOOT || status == IoTLabsStatus::WIFI_CONNECTING ||
             status == IoTLabsStatus::ACTIVATING || status == IoTLabsStatus::CLOUD_CONNECTING) {
    base = LED_BLUE; blink = true;
  } else {
    base = LED_GREEN; blink = false;
  }

  const bool relayOverlay = isRelayOn() && ((now % 3000) < 150);
  if (relayOverlay) {
    setLed(LED_WHITE);
    return;
  }

  if (blink) {
    setLed(((now / 500) % 2 == 0) ? base : LED_OFF_COLOR);
  } else {
    setLed(base);
  }
}

// ---- Relay control ------------------------------------------------------------

// Active HIGH here (matches the original: applyRelay(true) -> HIGH).
bool isRelayOn() { return digitalRead(RELAY_PIN) == HIGH; }

// Local direct write, independent of setPinMapping()'s own command handler
// -- used only by the button's toggle action below (a remote command from
// Console/Mobile now flows entirely through setPinMapping() instead).
void applyRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

void reportRelay(bool publishNow = true) {
  iot.report(CONTROL_ON_OFF, isRelayOn(), publishNow);
}

// B3/B4 (PRD): nhấn ngắn = đảo trạng thái tại chỗ; giữ >=10s = factory reset.
void updateBootButton() {
  const bool raw = digitalRead(BOOT_BUTTON_PIN);
  const uint32_t now = millis();

  if (raw != buttonLastRaw) {
    buttonLastRaw = raw;
    buttonLastChangeAt = now;
  }

  if (now - buttonLastChangeAt >= BUTTON_DEBOUNCE_MS && buttonStable != raw) {
    const bool wasPressed = (buttonStable == LOW);
    buttonStable = raw;
    const bool nowPressed = (buttonStable == LOW);

    if (nowPressed) {
      buttonPressStartAt = now;
      factoryResetTriggered = false;
    } else if (wasPressed && !factoryResetTriggered) {
      applyRelay(!isRelayOn());
      reportRelay();
      iot.report(CONTROL_BUTTON, "pressed");
    }
  }

  if (buttonStable == LOW && !factoryResetTriggered &&
      now - buttonPressStartAt >= FACTORY_RESET_HOLD_MS) {
    factoryResetTriggered = true;
    iot.factoryReset();
  }
}

// ---- Climate sensing ------------------------------------------------------------

void readAndReportClimate() {
  const float t = dht.readTemperature();
  const float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    if (consecutiveDhtErrors < 255) consecutiveDhtErrors++;
    sensorError = (consecutiveDhtErrors > DHT_ERROR_THRESHOLD);
    return;
  }

  consecutiveDhtErrors = 0;
  sensorError = false;
  iot.report(CONTROL_TEMPERATURE, t);
  iot.report(CONTROL_HUMIDITY, h);
}

// ---- setup() / loop() -------------------------------------------------------

void setup() {
  Serial.begin(115200);

  led.begin();
  setLed(LED_OFF_COLOR);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  buttonLastRaw = digitalRead(BOOT_BUTTON_PIN);
  buttonStable = buttonLastRaw;

  dht.begin();

  // Replaces the original's manual pinMode(OUTPUT) + safe-boot
  // digitalWrite(LOW) + iot.report(false, publishNow=false) +
  // iot.on(CONTROL_ON_OFF, onRelayControl) -- setPinMapping() does the
  // boot-glitch-safe init, caches the initial "off" state, and wires the
  // remote command handler in one call. Active HIGH relay (matches the
  // original), so activeLow stays default (false).
  iot.setPinMapping(CONTROL_ON_OFF, RELAY_PIN, IoTLabs::DIGITAL_OUT);
  iot.begin();  // macro-based convenience path — 1 Device Profile, no runtime branching needed

  readAndReportClimate();
  lastSensorAt = millis();
}

void loop() {
  iot.loop();
  updateLed();
  updateBootButton();

  const uint32_t now = millis();
  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
    lastSensorAt = now;
    readAndReportClimate();
  }
}
