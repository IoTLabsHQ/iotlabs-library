#pragma once

#include "../../core/ports/INetwork.h"

class Esp8266ArduinoNetwork : public INetwork {
 public:
  void beginStation() override;
  bool connect(const char* ssid, const char* password) override;
  void disconnect(bool eraseCredentials) override;
  bool stationConnected() const override;
  int scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) override;
};
