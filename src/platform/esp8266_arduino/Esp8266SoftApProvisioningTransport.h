#pragma once

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board, and headers get pulled in transitively even where the
// .cpp using them is guarded out -- so this whole declaration is an
// ESP8266-only no-op when parsed for another core.
#if defined(ARDUINO_ARCH_ESP8266)

// ESP8266WebServer.h ends with `using ESP8266WebServer =
// esp8266webserver::ESP8266WebServerTemplate<WiFiServer>;` -- there is no
// separate "WebServer" alias on this core the way there sometimes is on
// others, so the class is referenced by its real name here.
#include <ESP8266WebServer.h>

#include "../../core/ports/IProvisioningTransport.h"

class Esp8266SoftApProvisioningTransport : public IProvisioningTransport {
 public:
  Esp8266SoftApProvisioningTransport();

  bool startAccessPoint(const char* apName, const char* apPassword) override;
  void stopAccessPoint() override;
  bool accessPointActive() const override;

  void onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) override;
  void onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) override;
  void onProvisionRequest(ProvisioningRequestHandler handler, void* context) override;

  void poll() override;

 private:
  ESP8266WebServer _server;
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

#endif  // ARDUINO_ARCH_ESP8266
