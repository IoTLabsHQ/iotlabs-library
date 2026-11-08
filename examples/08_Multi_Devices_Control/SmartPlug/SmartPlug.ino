/*
  IoTLabs 08 — Multi Devices Control — SmartPlug

  ONE of 5 independent devices in this project (see
  IoTLabs internal design notes for the full PRD).
  Each device is its own IoTLabs Device — no gateway, no dependency on the
  other 4 devices. All 5 get merged onto ONE Dashboard purely as a Console
  UI action (see 01_SPEC_KIEN_TRUC_HE_THONG.md §2) — this firmware has no
  awareness of that and does not change based on it.

  Core controls only (see 02_PRD_CONTROLS_VA_AUTOMATION.md §2.1): on/off +
  physical button. Power-metering controls (voltage/current/power/energy/
  overload) are an OPTIONAL hardware extension, not implemented here — add
  a sensor read + iot.report() pair per PRD §2.1 if/when that module is
  wired in, same extension pattern already used by example 06 for its
  optional PM2.5 module.

  GPIO (ESP32-C3 Supermini):
    Relay control   GPIO4   (Active HIGH — verify jumper on real relay module)
    BOOT button     GPIO9   (short press = toggle relay, hold >=10s = factory
                             reset — same pattern as example 07)
    LED RGB WS2812  GPIO8   (onboard — verify against the actual board, clone
                             vendors vary)
    UART0 RX/TX     GPIO20/21 (USB-Serial, unused by app code)

  Provisioning page: built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport), no custom provisioning_page.h needed.
*/

#define IOTLABS_PROJECT_NAME "SmartPlug Setup"
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
constexpr uint8_t RELAY_PIN = 4;
constexpr uint8_t LED_PIN = 8;
constexpr uint8_t BOOT_BUTTON_PIN = 9;

// ---- Control IDs — match PROFILE_INTENT.json ----------------------------
constexpr char CONTROL_ON_OFF[] = "control_1_on_off";
constexpr char CONTROL_BUTTON[] = "control_2_button";

constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 50;

IoTLabs iot;
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

bool relayOn = false;
bool buttonLastRaw = HIGH;
bool buttonStable = HIGH;
uint32_t buttonLastChangeAt = 0;
uint32_t buttonPressStartAt = 0;
bool factoryResetTriggered = false;
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

// Same LED priority/state machine as example 07: error > provisioning >
// connecting > online, relay-ON overlay flashes white on top.
void updateLed() {
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

  if (relayOn && ((now % 3000) < 150)) {
    setLed(LED_WHITE);
    return;
  }

  if (blink) {
    setLed(((now / 500) % 2 == 0) ? base : LED_OFF_COLOR);
  } else {
    setLed(base);
  }
}

void applyRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, relayOn ? HIGH : LOW);
}

void reportRelay(bool publishNow = true) {
  iot.report(CONTROL_ON_OFF, relayOn, publishNow);
}

// Console/Mobile writes the on/off control -> apply immediately, report back.
void onRelayControl(bool on) {
  applyRelay(on);
  reportRelay();
}

// Short press = toggle at the outlet itself; hold >=10s = factory reset.
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
      applyRelay(!relayOn);
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

void setup() {
  Serial.begin(115200);

  led.begin();
  setLed(LED_OFF_COLOR);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // boot mặc định OFF — an toàn khi mất điện
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  buttonLastRaw = digitalRead(BOOT_BUTTON_PIN);
  buttonStable = buttonLastRaw;

  reportRelay(false);
  iot.on(CONTROL_ON_OFF, onRelayControl);
  iot.begin();
}

void loop() {
  iot.loop();
  updateLed();
  updateBootButton();
}
