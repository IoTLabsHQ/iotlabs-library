#include "Esp8266ArduinoNetwork.h"

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board; <ESP8266WiFi.h> only exists on the ESP8266 core.
#if defined(ARDUINO_ARCH_ESP8266)

#include <ESP8266WiFi.h>

#include <cstring>

void Esp8266ArduinoNetwork::beginStation() {
  const WiFiMode_t current = WiFi.getMode();
  const bool apActive = (current == WIFI_AP || current == WIFI_AP_STA);
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
}

bool Esp8266ArduinoNetwork::connect(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  return true;
}

// ESP8266's disconnect(wifioff, eraseCredentials) overload has the same
// two-parameter shape/semantics as ESP32's WiFi.disconnect(wifioff,
// eraseap): first param leaves the radio on, second actually wipes the
// stored station config when true.
void Esp8266ArduinoNetwork::disconnect(bool eraseCredentials) { WiFi.disconnect(false, eraseCredentials); }

bool Esp8266ArduinoNetwork::stationConnected() const { return WiFi.status() == WL_CONNECTED; }

int Esp8266ArduinoNetwork::scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) {
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
    results[i].secure = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  WiFi.scanDelete();
  return n;
}

#endif  // ARDUINO_ARCH_ESP8266
