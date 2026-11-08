/*
  IoTLabs Advanced 07 — Smart Home Complete (node template)

  Additional Arduino Library Manager dependencies:
    DHT sensor library by Adafruit
    Adafruit NeoPixel

  ONE firmware image, flashed identically onto every board deployed for this
  project — each becomes its own independent Device on IoTLabs Device
  Platform, no gateway, no dependency between boards. Unlike an earlier
  revision of this example, there is no runtime "role" branching: every
  board always has the SAME fixed capability set — 1 relay output + 1 DHT11
  sensor — reported unconditionally. Adding a sensor/actuator later means
  adding one more GPIO constant + report()/on() pair here and in
  PROFILE_INTENT.json, not inventing a new role value. See
  IoTLabs internal design notes
  and 02_PRD_CONTROLS_VA_AUTOMATION.md for the full spec this implements.

  GPIO (fixed/hardcoded, identical on every board):
    Relay control   GPIO4
    DHT11 data      GPIO5
    LED RGB WS2812  GPIO8   (onboard — verify against the actual board;
                             clone vendors vary, see spec §3)
    BOOT button     GPIO9   (short press = toggle relay, hold >=10s =
                             factory reset — same hold-timing pattern as
                             example 05; the library only exposes
                             factoryReset(), no built-in long-press)
    UART0 RX/TX     GPIO20/21 (USB-Serial, unused by app code)

  Provisioning page: this example no longer ships its own
  provisioning_page.h. The SoftAP setup page (WiFi SSID/password +
  activation token) is now a built-in IoTLabs Library feature
  (Esp32SoftApProvisioningTransport) — every example gets it for free.
  IOTLABS_PROJECT_NAME below sets its title; omit the #define to fall back
  to the library's own default ("IoTLabs Device Setup"). IOTLABS_DEVICE_ID
  is now required for begin() to succeed; IOTLABS_DEVICE_ACTIVATION_SECRET
  is optional and, when set, pre-fills the activation token on this page so
  you don't have to retype it on every reflash.
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

constexpr uint32_t SENSOR_INTERVAL_MS = 5000;       // C2 (PRD)
constexpr uint8_t DHT_ERROR_THRESHOLD = 5;          // C3 (PRD)
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr uint32_t FACTORY_RESET_HOLD_MS = 10000;   // B4 (PRD)
constexpr uint32_t LED_UPDATE_INTERVAL_MS = 50;

IoTLabs iot;
DHT dht(DHT_PIN, DHT11);
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---- Relay state -------------------------------------------------------------
bool relayOn = false;
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

// Priority (cao -> thấp, chỉ hiển thị 1 trạng thái tại 1 thời điểm — PRD §5):
// mất kết nối/lỗi cảm biến (đỏ nhấp nháy) > provisioning (vàng nhấp nháy) >
// đang kết nối (xanh dương nhấp nháy) > online (xanh lá) ; relay ON chớp
// trắng đè lên màu nền khi online.
void updateLed() {
  const uint32_t now = millis();
  if (now - lastLedUpdateAt < LED_UPDATE_INTERVAL_MS) return;
  lastLedUpdateAt = now;

  const IoTLabsStatus status = iot.status();
  LedColor base;
  bool blink = false;

  // MVP: lỗi đọc DHT11 kéo dài dùng chung màu đỏ với lỗi mạng — chưa có màu
  // riêng, xem Open Question #2 trong 05_KE_HOACH_IMPLEMENTATION.md.
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

  const bool relayOverlay = relayOn && ((now % 3000) < 150);
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

void applyRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, relayOn ? HIGH : LOW);
}

void reportRelay(bool publishNow = true) {
  iot.report(CONTROL_ON_OFF, relayOn, publishNow);
}

// B2 (PRD): client (Console/Mobile) ghi state -> set relay ngay, report lại.
void onRelayControl(bool on) {
  applyRelay(on);
  reportRelay();
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

// ---- Climate sensing ------------------------------------------------------------

// C2/C3 (PRD): đọc mỗi 5s; lỗi giữ giá trị report gần nhất, không report rác;
// >5 lỗi liên tiếp -> sensorError (LED đỏ, dùng chung state với lỗi mạng).
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

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // B1: boot mặc định OFF
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  buttonLastRaw = digitalRead(BOOT_BUTTON_PIN);
  buttonStable = buttonLastRaw;

  dht.begin();

  reportRelay(false);

  iot.on(CONTROL_ON_OFF, onRelayControl);
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
