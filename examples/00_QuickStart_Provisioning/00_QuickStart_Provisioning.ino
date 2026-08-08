/*
  IoTLabs QuickStart 00 — Provisioning

  Purpose:
    Verify the IoTLabs SDK boots and exposes the provisioning SoftAP.

  Before upload:
    1. Create/choose a Device Profile in IoTLabs Mobile / Device Platform.
    2. Replace IOTLABS_PROFILE_ID below.
    3. Set the Device Platform URL.

  Development note:
    IOTLABS_TLS_INSECURE is enabled here only so the sketch can be used
    with a development Device Platform before production CA certificates
    are configured. Remove it for production firmware.
*/

#define IOTLABS_PROFILE_ID "PROF_REPLACE_ME"
#define IOTLABS_PROFILE_VERSION 1
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_PLATFORM_URL "https://YOUR_DEVICE_PLATFORM_HOST"
#define IOTLABS_TLS_INSECURE true

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
