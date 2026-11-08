#include "Stm32ArduinoNetwork.h"

#if defined(ARDUINO_ARCH_STM32)

// See Stm32ProvisioningTransport.h for why LwIP.h must precede STM32Ethernet.h.
#include <LwIP.h>
#include <STM32Ethernet.h>

namespace {
// Bounded DHCP negotiation budget for Ethernet.begin(). Unlike WiFi.begin()
// (which returns immediately and is polled via stationConnected()),
// STM32Ethernet's Ethernet.begin() blocks synchronously for up to this long
// while it runs the DHCP handshake. There is no non-blocking/incremental
// DHCP entry point in this library version, so INetwork::connect() blocking
// here is a real, documented adapter limitation rather than a port-contract
// violation (the port itself does not promise non-blocking connect()).
constexpr unsigned long kDhcpTimeoutMs = 15000;
constexpr unsigned long kDhcpResponseTimeoutMs = 4000;
}  // namespace

void Stm32ArduinoNetwork::beginStation() {
  // No station/AP "mode" concept on a wired link (unlike WiFi.mode()):
  // the Ethernet PHY is always "station-like". Nothing to prepare here
  // before connect() brings up DHCP.
}

bool Stm32ArduinoNetwork::connect(const char* ssid, const char* password) {
  // ssid/password are intentionally ignored: a wired Ethernet link has no
  // equivalent of WiFi credentials. Per
  // IoTLabs internal design notes, INetwork must not
  // assume WiFi; this adapter simply brings up DHCP on the wired PHY.
  (void)ssid;
  (void)password;

  // Passing a NULL mac_address lets STM32Ethernet derive a locally-
  // administered MAC address internally (documented STM32Ethernet usage
  // pattern); no separate hardware MAC EEPROM exists on this board.
  const int result = Ethernet.begin(kDhcpTimeoutMs, kDhcpResponseTimeoutMs);
  _dhcpActive = (result == 1);
  return _dhcpActive;
}

void Stm32ArduinoNetwork::disconnect(bool eraseCredentials) {
  // STM32Ethernet has no disconnect()/link-teardown API (there is no
  // "association" to drop, unlike WiFi) and there are no cached Wi-Fi
  // credentials at the radio-driver level to erase on a wired link, so
  // eraseCredentials is intentionally unused — IStorage/ISecretStore own
  // credential lifecycle at the IoTLabs layer regardless of platform.
  (void)eraseCredentials;
  _dhcpActive = false;
}

bool Stm32ArduinoNetwork::stationConnected() const {
  // No dedicated "service the stack" hook exists on INetwork, and
  // stationConnected() is the one INetwork method ConnectionManager polls
  // every loop() tick (see src/core/ConnectionManager.cpp), so this is the
  // natural place to keep LwIP's Ethernet.schedule() serviced and the DHCP
  // lease maintained via Ethernet.maintain() — both are cheap, non-blocking
  // calls safe to run every tick.
  Ethernet.schedule();
  Ethernet.maintain();
  return _dhcpActive && Ethernet.linkStatus() != LinkOFF;
}

int Stm32ArduinoNetwork::scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) {
  // Not applicable to Ethernet: there is no radio to scan. Returning 0
  // (rather than -1/failure) matches "no networks found", which is the
  // most honest mapping for a provisioning UI that lists WiFi networks on
  // a board that has none to list.
  (void)results;
  (void)maxResults;
  return 0;
}

#endif  // ARDUINO_ARCH_STM32
