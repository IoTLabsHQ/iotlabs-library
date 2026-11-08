#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "../../core/ports/INetwork.h"

class Esp32ArduinoNetwork : public INetwork {
 public:
  void beginStation() override;
  bool connect(const char* ssid, const char* password) override;
  void disconnect(bool eraseCredentials) override;
  bool stationConnected() const override;
  int scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) override;
};

#endif  // ARDUINO_ARCH_ESP32
