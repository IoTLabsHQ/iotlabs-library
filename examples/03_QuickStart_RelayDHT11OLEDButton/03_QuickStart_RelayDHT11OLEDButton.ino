/*
  IoTLabs QuickStart 03 — Relay + DHT11 + OLED + Button

  Additional Arduino Library Manager dependencies:
    DHT sensor library by Adafruit
    Adafruit GFX Library
    Adafruit SSD1306

  ESP32-C3 reference pins:
    Relay       GPIO3, Active LOW
    DHT11       GPIO4
    OLED SDA    GPIO8
    OLED SCL    GPIO9
    Button      GPIO10 -> GND, INPUT_PULLUP
*/

#define IOTLABS_PROFILE_ID "PROF_REPLACE_ME"
#define IOTLABS_PROFILE_VERSION 1
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_PLATFORM_URL "https://YOUR_DEVICE_PLATFORM_HOST"
#define IOTLABS_TLS_INSECURE true   // Development only. Configure CA for production.

#include <IoTLabs.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr uint8_t RELAY_PIN = 3;
constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t OLED_SDA = 8;
constexpr uint8_t OLED_SCL = 9;
constexpr uint8_t BUTTON_PIN = 10;
constexpr uint8_t DHT_TYPE = DHT11;

constexpr char CONTROL_RELAY[] = "control_1_on_off";
constexpr char CONTROL_TEMPERATURE[] = "control_2_value";
constexpr char CONTROL_HUMIDITY[] = "control_3_value";
constexpr char CONTROL_BUTTON[] = "control_4_button";

constexpr uint32_t SENSOR_INTERVAL_MS = 2500;
constexpr uint32_t DISPLAY_INTERVAL_MS = 300;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;

IoTLabs iot;
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

bool relayOn = false;
bool oledReady = false;
float temperature = NAN;
float humidity = NAN;
bool lastButtonRaw = HIGH;
bool buttonStable = HIGH;
uint32_t lastButtonChangeAt = 0;
uint32_t lastSensorAt = 0;
uint32_t lastDisplayAt = 0;

void setRelay(bool enabled) {
  relayOn = enabled;
  digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);
  iot.report(CONTROL_RELAY, relayOn);
}

void onRelayControl(bool value) {
  setRelay(value);
}

void triggerButtonAction() {
  setRelay(!relayOn);

  JsonDocument value;
  value["state"] = "pressed";
  value["action"] = "toggle_relay";
  iot.report(CONTROL_BUTTON, value.as<JsonVariantConst>());
}

void onRemoteButton(const IoTLabsValue& value) {
  if (value.isObject() && value.has("state") && value["state"].asString() == "pressed") {
    triggerButtonAction();
  }
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

void readDht11() {
  const float h = dht.readHumidity();
  const float t = dht.readTemperature();

  if (isnan(t) || isnan(h)) return;

  temperature = t;
  humidity = h;

  iot.report(CONTROL_TEMPERATURE, temperature);
  iot.report(CONTROL_HUMIDITY, humidity);
}

void updateDisplay() {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("IoTLabs QuickStart");

  display.setCursor(0, 16);
  display.print("T: ");
  if (isnan(temperature)) display.print("--"); else display.print(temperature, 1);
  display.println(" C");

  display.print("H: ");
  if (isnan(humidity)) display.print("--"); else display.print(humidity, 1);
  display.println(" %");

  display.print("Relay: ");
  display.println(relayOn ? "ON" : "OFF");

  if (!iot.provisioned()) {
    display.print("Setup: ");
    display.println(iot.provisioningApName());
  } else {
    display.print("Cloud: ");
    display.println(iot.connected() ? "ONLINE" : "OFFLINE");
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Safe boot OFF.

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastButtonRaw = digitalRead(BUTTON_PIN);
  buttonStable = lastButtonRaw;

  dht.begin();

  Wire.begin(OLED_SDA, OLED_SCL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  iot.report(CONTROL_RELAY, false, false);
  iot.on(CONTROL_RELAY, onRelayControl);
  iot.on(CONTROL_BUTTON, onRemoteButton);

  iot.begin();
}

void loop() {
  iot.loop();

  // Local hardware remains responsive even when Cloud is offline.
  updateButton();

  const uint32_t now = millis();

  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
    lastSensorAt = now;
    readDht11();
  }

  if (now - lastDisplayAt >= DISPLAY_INTERVAL_MS) {
    lastDisplayAt = now;
    updateDisplay();
  }
}
