#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include "../../core/ports/INetwork.h"

// Wired Ethernet adapter for a Nucleo-144 board with an on-board PHY (e.g.
// NUCLEO_F429ZI), backed by the STM32duino-maintained STM32Ethernet+LwIP
// stack. See IoTLabs internal design notes: "STM32 is not
// equivalent to STM32 Wi-Fi" — this is exactly why the core depends on
// INetwork, not IWiFi. connect()'s ssid/password parameters have no
// equivalent on a wired link and are intentionally ignored (see .cpp);
// scanNetworks() is a WiFi-only concept and always returns 0 here.
class Stm32ArduinoNetwork : public INetwork {
 public:
  void beginStation() override;
  bool connect(const char* ssid, const char* password) override;
  void disconnect(bool eraseCredentials) override;
  bool stationConnected() const override;
  int scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) override;

 private:
  bool _dhcpActive = false;
};

#endif  // ARDUINO_ARCH_STM32
