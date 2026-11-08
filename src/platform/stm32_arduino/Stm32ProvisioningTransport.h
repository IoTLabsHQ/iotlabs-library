#pragma once

#if defined(ARDUINO_ARCH_STM32)

// LwIP.h must be included before STM32Ethernet.h (official STM32duino
// example ordering, e.g. STM32duino_STM32Ethernet/examples/WebClientRepeating):
// STM32Ethernet's headers pull in lwip/*.h paths that only resolve once the
// LwIP library's src/ root has been added to the include path.
#include <LwIP.h>
#include <STM32Ethernet.h>

#include <string>

#include "../../core/ports/IProvisioningTransport.h"

// KNOWN ARCHITECTURE DEVIATION (see IoTLabs internal design notes,
// "Provisioning" section, for the full explanation):
//
// ADR-006-PHASE1-PORTABLE-CORE.md folds "local AP + local HTTP server" into
// one IProvisioningTransport port because on every WiFi-based board those
// two things are one cohesive mechanism (WiFi.softAP() + WebServer). A
// wired Ethernet PHY cannot originate its own isolated network the way a
// WiFi radio can create a SoftAP — there is nothing to become an access
// point *of*. So on this adapter, startAccessPoint(apName, apPassword)
// does NOT create an isolated hotspot: apName/apPassword are accepted (for
// interface compatibility) but ignored, and instead this call brings up
// the same DHCP-assigned LAN connection Stm32ArduinoNetwork::connect()
// would (idempotently — calling Ethernet.begin() twice across the two
// independent port instances is harmless, just a redundant DHCP
// negotiation) and starts a small hand-rolled HTTP/1.1 request/response
// listener (EthernetServer) reachable at that DHCP IP on the same wired
// LAN. There is no captive-portal AP name to join and no mDNS resolution
// implemented, so a provisioning client must already be on the same LAN
// and learn the device's IP out-of-band (e.g. the Stm32ArduinoLogger
// Serial output, or a static DHCP reservation) — a real, documented
// limitation of provisioning-before-network-is-known on a wired link.
class Stm32ProvisioningTransport : public IProvisioningTransport {
 public:
  Stm32ProvisioningTransport();

  bool startAccessPoint(const char* apName, const char* apPassword) override;
  void stopAccessPoint() override;
  bool accessPointActive() const override;

  void onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) override;
  void onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) override;
  void onProvisionRequest(ProvisioningRequestHandler handler, void* context) override;

  void poll() override;

 private:
  EthernetServer _server;
  bool _active = false;

  ProvisioningRequestHandler _deviceInfoHandler = nullptr;
  void* _deviceInfoContext = nullptr;
  ProvisioningRequestHandler _wifiNetworksHandler = nullptr;
  void* _wifiNetworksContext = nullptr;
  ProvisioningRequestHandler _provisionHandler = nullptr;
  void* _provisionContext = nullptr;

  std::string _requestBody;

  void handleClient(EthernetClient& client);
  void dispatch(EthernetClient& client, ProvisioningRequestHandler handler, void* context, const std::string& body);
  void sendNotFound(EthernetClient& client);
};

#endif  // ARDUINO_ARCH_STM32
