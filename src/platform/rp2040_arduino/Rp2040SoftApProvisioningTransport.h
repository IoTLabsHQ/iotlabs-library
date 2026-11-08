#pragma once

#include "../../core/ports/IProvisioningTransport.h"

// See Rp2040ArduinoMqttTransport.h for why this header guards its
// platform-only include/body.
#if defined(ARDUINO_ARCH_RP2040)

#include <WebServer.h>

class Rp2040SoftApProvisioningTransport : public IProvisioningTransport {
 public:
  Rp2040SoftApProvisioningTransport();

  bool startAccessPoint(const char* apName, const char* apPassword) override;
  void stopAccessPoint() override;
  bool accessPointActive() const override;

  void onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) override;
  void onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) override;
  void onProvisionRequest(ProvisioningRequestHandler handler, void* context) override;

  void poll() override;

 private:
  WebServer _server;
  bool _active = false;
  bool _routesRegistered = false;

  ProvisioningRequestHandler _deviceInfoHandler = nullptr;
  void* _deviceInfoContext = nullptr;
  ProvisioningRequestHandler _wifiNetworksHandler = nullptr;
  void* _wifiNetworksContext = nullptr;
  ProvisioningRequestHandler _provisionHandler = nullptr;
  void* _provisionContext = nullptr;

  void registerRoutesOnce();
  void dispatch(ProvisioningRequestHandler handler, void* context, bool withBody);
  void handleNotFound();
};

#endif  // ARDUINO_ARCH_RP2040
