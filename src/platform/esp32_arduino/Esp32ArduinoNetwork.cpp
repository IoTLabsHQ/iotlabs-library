#include "Esp32ArduinoNetwork.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFi.h>
#include <cstring>

void Esp32ArduinoNetwork::beginStation() {
  const wifi_mode_t current = WiFi.getMode();
  const bool apActive = (current == WIFI_AP || current == WIFI_AP_STA);
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
}

bool Esp32ArduinoNetwork::connect(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  return true;
}

void Esp32ArduinoNetwork::disconnect(bool eraseCredentials) { WiFi.disconnect(false, eraseCredentials); }

bool Esp32ArduinoNetwork::stationConnected() const { return WiFi.status() == WL_CONNECTED; }

int Esp32ArduinoNetwork::scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) {
  const int count = WiFi.scanNetworks(false, true);
  if (count <= 0) {
    WiFi.scanDelete();
    return count < 0 ? -1 : 0;
  }

  const int n = count < maxResults ? count : maxResults;
  for (int i = 0; i < n; ++i) {
    const String ssid = WiFi.SSID(i);
    strncpy(results[i].ssid, ssid.c_str(), IoTLabsNetworkScanResult::SSID_MAX - 1);
    results[i].ssid[IoTLabsNetworkScanResult::SSID_MAX - 1] = '\0';
    results[i].rssi = WiFi.RSSI(i);
    results[i].secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  return n;
}

#endif  // ARDUINO_ARCH_ESP32
