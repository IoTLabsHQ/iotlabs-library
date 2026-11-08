/*
  IoTLabs QuickStart 00 — Provisioning

  Purpose:
    Verify the IoTLabs SDK boots and exposes the provisioning SoftAP.

  Before upload:
    1. Create/choose a Device Profile in IoTLabs Mobile / Device Platform.
    2. Replace IOTLABS_DEVICE_TEMPLATE_ID below.
    3. Set the Device Platform URL.
*/

#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_REPLACE_ME"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 1
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_DEVICE_ACTIVATION_SECRET "paste-the-one-time-activation-token-from-Console-here"  // optional: skip retyping the token into the SoftAP portal while iterating
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>

IoTLabs iot;

void onIoTLabsEvent(IoTLabsEvent event) {
  if (event == IoTLabsEvent::PROVISIONING_STARTED) {
    Serial.println();
    Serial.println("[QuickStart] Device is ready for provisioning.");
    Serial.print("[QuickStart] Connect Mobile to: ");
    Serial.println(iot.provisioningApName());
  }

  if (event == IoTLabsEvent::ONLINE) {
    Serial.println("[QuickStart] IoTLabs Device ONLINE");
    Serial.print("[QuickStart] Device ID: ");
    Serial.println(iot.deviceId());
  }

  if (event == IoTLabsEvent::ERROR) {
    Serial.print("[QuickStart] Error: ");
    Serial.println(iot.lastError());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  iot.onEvent(onIoTLabsEvent);

  if (!iot.begin()) {
    Serial.print("[QuickStart] begin() failed: ");
    Serial.println(iot.lastError());
  }
}

void loop() {
  iot.loop();
}
