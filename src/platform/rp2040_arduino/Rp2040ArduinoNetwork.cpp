#include "Rp2040ArduinoNetwork.h"

#if defined(ARDUINO_ARCH_RP2040)

#include <WiFi.h>
#include <cstring>

void Rp2040ArduinoNetwork::beginStation() {
  const WiFiMode_t current = WiFi.getMode();
  const bool apActive = (current == WIFI_AP || current == WIFI_AP_STA);
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  // arduino-pico's WiFiClass has no setAutoReconnect() (ESP32-only extra);
  // persistent() is kept as an explicit no-op ESP8266-compat stub upstream,
  // called here only for parity with the ESP32 adapter's intent (no
  // radio-level credential cache to disable on this core).
  WiFi.persistent(false);
}

bool Rp2040ArduinoNetwork::connect(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  return true;
}

void Rp2040ArduinoNetwork::disconnect(bool eraseCredentials) {
  // arduino-pico's WiFi.disconnect(bool wifi_off) has no second
  // "erase cached credentials" parameter: unlike ESP-IDF, this core does not
  // cache the last SSID/password in its own NVS-equivalent store, so there is
  // nothing extra to erase at the radio-driver level (IStorage/ISecretStore
  // already own that responsibility at the IoTLabs layer).
  (void)eraseCredentials;
  WiFi.disconnect(false);
}

bool Rp2040ArduinoNetwork::stationConnected() const { return WiFi.status() == WL_CONNECTED; }

int Rp2040ArduinoNetwork::scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) {
  const int count = WiFi.scanNetworks();
  if (count <= 0) {
    WiFi.scanDelete();
    return count < 0 ? -1 : 0;
  }

  const int n = count < maxResults ? count : maxResults;
  for (int i = 0; i < n; ++i) {
    const char* ssid = WiFi.SSID(i);
    strncpy(results[i].ssid, ssid ? ssid : "", IoTLabsNetworkScanResult::SSID_MAX - 1);
    results[i].ssid[IoTLabsNetworkScanResult::SSID_MAX - 1] = '\0';
    results[i].rssi = WiFi.RSSI(i);
    results[i].secure = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  WiFi.scanDelete();
  return n;
}

#endif  // ARDUINO_ARCH_RP2040
