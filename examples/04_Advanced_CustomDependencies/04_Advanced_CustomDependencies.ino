/*
  IoTLabs Advanced 04 — Custom Dependencies

  Demonstrates the advanced composition constructor added in the Phase 1
  ports/adapters refactor:

    IoTLabs iot(dependencies);

  Every capability IoTLabs needs (clock, network, MQTT transport, storage,
  ...) is a small port interface under core/ports/. Normal sketches never
  see this: `IoTLabs iot;` selects the default ESP32 Arduino adapters for
  you. This sketch instead reuses those default adapters for everything
  EXCEPT logging, and supplies its own ILogger that prefixes every SDK log
  line and blinks the onboard LED once per line — showing how an
  application (or a unit test) can substitute a single capability without
  forking or patching the library.

  Board reference: ESP32-C3 SuperMini
  Relay control: GPIO3, Active LOW
  Onboard LED:   GPIO8 (many ESP32-C3 SuperMini boards; adjust if different)

  Cloud Control:
    control_1_on_off
*/

#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>
#include <core/ports/ILogger.h>
#include <platform/esp32_arduino/Esp32ArduinoPlatform.h>

constexpr uint8_t RELAY_PIN = 3;
constexpr uint8_t STATUS_LED_PIN = 8;
constexpr char CONTROL_RELAY[] = "control_1_on_off";

// A custom port implementation supplied by application code, not the
// library. Any of the 9 ports under core/ports/ can be overridden this way
// — e.g. an IStorage backed by an external secure element, or an IClock
// double used by a host-side unit test.
class PrefixLogger : public ILogger {
 public:
  void log(const char* line) override {
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    Serial.print("[MyDevice] ");
    Serial.println(line);
  }
};

Esp32ArduinoPlatform platform;  // owns the default ESP32 adapters
PrefixLogger prefixLogger;      // the one port we override

IoTLabs iot(([]() {
  IoTLabsDependencies deps = platform.dependencies();
  deps.logger = &prefixLogger;
  return deps;
})());

bool relayOn = false;

void setRelay(bool enabled) {
  relayOn = enabled;
  digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);  // Active LOW relay.
  iot.report(CONTROL_RELAY, relayOn);
}

void onRelayControl(bool value) { setRelay(value); }

void onIoTLabsEvent(IoTLabsEvent event) {
  if (event == IoTLabsEvent::PROVISIONING_STARTED) {
    Serial.print("[App] Provision using AP: ");
    Serial.println(iot.provisioningApName());
  } else if (event == IoTLabsEvent::ONLINE) {
    Serial.println("[App] ONLINE");
  } else if (event == IoTLabsEvent::ERROR) {
    Serial.print("[App] Error: ");
    Serial.println(iot.lastError());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Safe boot OFF before networking starts.

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  iot.report(CONTROL_RELAY, false, false);
  iot.on(CONTROL_RELAY, onRelayControl);
  iot.onEvent(onIoTLabsEvent);

  iot.begin();
}

void loop() { iot.loop(); }
