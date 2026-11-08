/*
  IoTLabs QuickStart 01 — Relay (AutoPinMapping variant)

  Same device behavior as examples/01_QuickStart_Relay, rewritten with
  iot.setPinMapping() instead of hand-written pinMode/digitalWrite/report
  wiring. setPinMapping() is entirely optional -- see the original example
  for the manual equivalent.

  Board reference: ESP32-C3 SuperMini
  Relay control: GPIO3, Active LOW

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

constexpr uint8_t RELAY_PIN = 3;
constexpr char CONTROL_RELAY[] = "control_1_on_off";

IoTLabs iot;

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

  iot.onEvent(onIoTLabsEvent);

  // Replaces the original's manual pinMode(OUTPUT) + safe-boot
  // digitalWrite(HIGH) + iot.report(false, publishNow=false) + iot.on() --
  // setPinMapping() does the boot-glitch-safe init, caches the initial
  // "off" state, and wires the command handler in one call. Active LOW
  // relay, so activeLow=true.
  iot.setPinMapping(CONTROL_RELAY, RELAY_PIN, IoTLabs::DIGITAL_OUT, /*activeLow*/ true);

  iot.begin();
}

void loop() {
  iot.loop();
}
