/*
  IoTLabs QuickStart 02 — DHT11

  Additional Arduino Library Manager dependency:
    DHT sensor library by Adafruit

  Board reference: ESP32-C3 SuperMini
  DHT11 DATA: GPIO4

  Controls:
    control_1_value = temperature
    control_2_value = humidity
*/

#define IOTLABS_PROFILE_ID "PROF_REPLACE_ME"
#define IOTLABS_PROFILE_VERSION 1
#define IOTLABS_FIRMWARE_VERSION "1.0.0"
#define IOTLABS_DEVICE_PLATFORM_URL "https://YOUR_DEVICE_PLATFORM_HOST"
#define IOTLABS_TLS_INSECURE true   // Development only. Configure CA for production.

#include <IoTLabs.h>
#include <DHT.h>

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT11;
constexpr uint32_t SENSOR_INTERVAL_MS = 2500;

constexpr char CONTROL_TEMPERATURE[] = "control_1_value";
constexpr char CONTROL_HUMIDITY[] = "control_2_value";

IoTLabs iot;
DHT dht(DHT_PIN, DHT_TYPE);
uint32_t lastReadAt = 0;

void readSensor() {
  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT11] Read failed");
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  iot.report(CONTROL_TEMPERATURE, temperature);
  iot.report(CONTROL_HUMIDITY, humidity);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  dht.begin();

  iot.onEvent([](IoTLabsEvent event) {
    if (event == IoTLabsEvent::PROVISIONING_STARTED) {
      Serial.print("[IoTLabs] Provision using AP: ");
      Serial.println(iot.provisioningApName());
    }
  });

  iot.begin();
}

void loop() {
  iot.loop();

  const uint32_t now = millis();
  if (now - lastReadAt >= SENSOR_INTERVAL_MS) {
    lastReadAt = now;
    readSensor();
  }
}
