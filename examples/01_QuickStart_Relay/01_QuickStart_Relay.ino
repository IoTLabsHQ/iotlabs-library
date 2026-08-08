/*
  IoTLabs QuickStart 01 — Relay

  Board reference: ESP32-C3 SuperMini
  Relay control: GPIO3, Active LOW

  Cloud Control:
    control_1_on_off
*/

#define IOTLABS_PROFILE_ID "PROF_REPLACE_ME"
#define IOTLABS_PROFILE_VERSION 1
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_PLATFORM_URL "https://YOUR_DEVICE_PLATFORM_HOST"
#define IOTLABS_TLS_INSECURE true   // Development only. Configure CA for production.

#include <IoTLabs.h>

constexpr uint8_t RELAY_PIN = 3;
constexpr char CONTROL_RELAY[] = "control_1_on_off";

IoTLabs iot;
bool relayOn = false;

void setRelay(bool enabled) {
  relayOn = enabled;

  // Active LOW relay.
  digitalWrite(RELAY_PIN, relayOn ? LOW : HIGH);

  // Report actual hardware state back to IoTLabs.
  iot.report(CONTROL_RELAY, relayOn);
}

void onRelayControl(bool value) {
  setRelay(value);
}

void onIoTLabsEvent(IoTLabsEvent event) {
  if (event == IoTLabsEvent::PROVISIONING_STARTED) {
    Serial.print("[IoTLabs] Provision using AP: ");
    Serial.println(iot.provisioningApName());
  } else if (event == IoTLabsEvent::ONLINE) {
    Serial.println("[IoTLabs] ONLINE");
  } else if (event == IoTLabsEvent::ERROR) {
    Serial.print("[IoTLabs] Error: ");
    Serial.println(iot.lastError());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(RELAY_PIN, OUTPUT);

  // Safe boot: relay OFF before networking starts.
  digitalWrite(RELAY_PIN, HIGH);

  // Cache initial reported state without publishing yet.
  iot.report(CONTROL_RELAY, false, false);

  iot.on(CONTROL_RELAY, onRelayControl);
  iot.onEvent(onIoTLabsEvent);

  iot.begin();
}

void loop() {
  iot.loop();
}
