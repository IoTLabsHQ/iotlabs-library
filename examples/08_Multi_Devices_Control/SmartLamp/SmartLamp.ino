/*
  IoTLabs 08 — Multi Devices Control — SmartLamp

  ONE of 5 independent devices in this project (see
  IoTLabs internal design notes for the full PRD).
  Independent from the other 4 devices — no gateway, no shared state.

  Controls (see 02_PRD_CONTROLS_VA_AUTOMATION.md §2.2):
    control_1_on_off  on/off
    control_2_value   brightness 0-100% (gauge, read_write) — PWM dimmed LED
    control_3_select  lighting mode preset (reading/relax/night/eco)
    control_4_button  physical button (short press = toggle on/off)

  RGB/CCT color controls are an OPTIONAL hardware extension, not implemented
  here (see PRD §2.2) — same "add module later" pattern as example 06's
  optional PM2.5 sensor.

  Hardware note: this models a PWM-dimmed load (LED strip via a MOSFET/driver
  module on the PWM pin), not a mains AC dimmer — a real TRIAC dimmer needs
  dedicated hardware and is out of scope for this library example.

  GPIO (ESP32-C3 Supermini):
    PWM load        GPIO4   (ledc, 5kHz/8-bit — drives an external MOSFET/LED
                             driver module, not mains voltage directly)
    BOOT button      GPIO9   (short press = toggle lamp, hold >=10s = factory
                              reset — same pattern as example 07)
    LED RGB WS2812   GPIO8   (onboard status LED — verify against real board)

  Provisioning page: built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport), no custom provisioning_page.h needed.
*/

#define IOTLABS_PROJECT_NAME "SmartLamp Setup"
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
constexpr uint8_t PWM_PIN = 4;
constexpr uint8_t STATUS_LED_PIN = 8;
constexpr uint8_t BOOT_BUTTON_PIN = 9;

constexpr uint32_t PWM_FREQ_HZ = 5000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;  // duty 0-255

// ---- Control IDs — match PROFILE_INTENT.json ----------------------------
constexpr char CONTROL_ON_OFF[] = "control_1_on_off";
constexpr char CONTROL_BRIGHTNESS[] = "control_2_value";
constexpr char CONTROL_MODE[] = "control_3_select";
constexpr char CONTROL_BUTTON[] = "control_4_button";

constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 50;

// Lighting-mode presets — brightness (%) each mode sets when selected.
struct LightingMode { const char* name; uint8_t brightnessPct; };
constexpr LightingMode LIGHTING_MODES[] = {
  {"reading", 100},
  {"relax", 60},
  {"night", 15},
  {"eco", 40},
};
constexpr size_t LIGHTING_MODE_COUNT = sizeof(LIGHTING_MODES) / sizeof(LIGHTING_MODES[0]);

IoTLabs iot;
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

bool lampOn = false;
uint8_t brightnessPct = 100;       // last brightness while ON
uint8_t lastNonZeroBrightness = 100;
char currentMode[16] = "reading";

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

void setStatusLed(const LedColor& c) {
  statusLed.setPixelColor(0, statusLed.Color(c.r, c.g, c.b));
  statusLed.show();
}

// Same LED priority state machine as examples 07/SmartPlug: error >
// provisioning > connecting > online (steady green, no relay-overlay here —
// the PWM load itself is the visible "on" indicator).
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

void applyPwm() {
  const uint8_t effectivePct = lampOn ? brightnessPct : 0;
  const uint32_t duty = (static_cast<uint32_t>(effectivePct) * 255U) / 100U;
  ledcWrite(PWM_PIN, duty);
}

void reportState(bool publishNow = true) {
  iot.report(CONTROL_ON_OFF, lampOn, publishNow);
  iot.report(CONTROL_BRIGHTNESS, static_cast<int>(brightnessPct), publishNow);
}

void applyOnOff(bool on) {
  lampOn = on;
  if (on && brightnessPct == 0) brightnessPct = lastNonZeroBrightness;
  applyPwm();
}

void applyBrightness(uint8_t pct) {
  brightnessPct = pct;
  if (pct > 0) lastNonZeroBrightness = pct;
  lampOn = (pct > 0);
  applyPwm();
}

// Console/Mobile writes on/off -> apply, report both controls back (brightness
// display should reflect the "last known level", so report it too).
void onOnOffControl(bool on) {
  applyOnOff(on);
  reportState();
}

// Console/Mobile writes brightness (0-100) -> apply, report both back.
void onBrightnessControl(float value) {
  const uint8_t pct = static_cast<uint8_t>(constrain(value, 0.0f, 100.0f));
  applyBrightness(pct);
  reportState();
}

// Console/Mobile writes lighting mode (string, one of LIGHTING_MODES) ->
// apply the mode's preset brightness, report mode + brightness + on/off.
void onModeControl(const String& value) {
  for (size_t i = 0; i < LIGHTING_MODE_COUNT; i++) {
    if (value == LIGHTING_MODES[i].name) {
      strncpy(currentMode, LIGHTING_MODES[i].name, sizeof(currentMode) - 1);
      currentMode[sizeof(currentMode) - 1] = '\0';
      applyBrightness(LIGHTING_MODES[i].brightnessPct);
      iot.report(CONTROL_MODE, currentMode);
      reportState();
      return;
    }
  }
}

// Short press = toggle lamp; hold >=10s = factory reset.
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
      applyOnOff(!lampOn);
      reportState();
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

  statusLed.begin();
  setStatusLed(LED_OFF_COLOR);

  ledcAttach(PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
  applyPwm();  // boot mặc định OFF (lampOn = false)

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  buttonLastRaw = digitalRead(BOOT_BUTTON_PIN);
  buttonStable = buttonLastRaw;

  reportState(false);
  iot.report(CONTROL_MODE, currentMode, false);

  iot.on(CONTROL_ON_OFF, onOnOffControl);
  iot.on(CONTROL_BRIGHTNESS, onBrightnessControl);
  iot.on(CONTROL_MODE, onModeControl);
  iot.begin();
}

void loop() {
  iot.loop();
  updateStatusLed();
  updateBootButton();
}
