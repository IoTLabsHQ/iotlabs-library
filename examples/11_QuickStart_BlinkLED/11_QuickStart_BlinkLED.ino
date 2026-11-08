#define IOTLABS_DEVICE_TEMPLATE_ID "PROF_c4ef4ed5"
#define IOTLABS_DEVICE_TEMPLATE_VERSION 2
#define IOTLABS_PROVISIONING_URL "https://vn.iotlabs.vn"
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_PROJECT_NAME "ESP32 Blink LED - 001 (Prod) Profile"
#define IOTLABS_DEVICE_ID "DEV_your_device_id_here"  // REQUIRED: replace with your own Device's id from Console (create a Device there first — id looks like DEV_xxxxxxxx)
// #define IOTLABS_DEVICE_ACTIVATION_SECRET "paste-the-one-time-activation-token-from-Console-here"  // optional: skip retyping the token into the SoftAP portal while iterating
// #define IOTLABS_WIFI_SSID "your-wifi-ssid"        // optional: combined with the two macros above, skips the SoftAP portal entirely
// #define IOTLABS_WIFI_PASSWORD "your-wifi-password" // leave undefined/empty for an open network

#include <IoTLabs.h>

constexpr uint8_t LED_1_PIN = 8;
constexpr char CONTROL_1[] = "control_1_on_off";

bool LED_1On = false;

IoTLabs iot;

void setLED_1(bool enabled) {
  LED_1On = enabled;
  digitalWrite(LED_1_PIN, LED_1On ? LOW : HIGH);
  iot.report(CONTROL_1, LED_1On);
}

void onLED_1Control(bool value) {
  setLED_1(value);
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
  iot.onEvent(onIoTLabsEvent);
  pinMode(LED_1_PIN, OUTPUT);
  digitalWrite(LED_1_PIN, HIGH); // safe boot: off
  iot.report(CONTROL_1, false, false); // cache, don't publish yet
  iot.on(CONTROL_1, onLED_1Control);
  iot.begin();
}

void loop() {
  iot.loop();
}
