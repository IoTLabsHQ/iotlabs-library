#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <WebServer.h>

#include "../../core/ports/IProvisioningTransport.h"

class Esp32SoftApProvisioningTransport : public IProvisioningTransport {
 public:
  Esp32SoftApProvisioningTransport();

  bool startAccessPoint(const char* apName, const char* apPassword) override;
  void stopAccessPoint() override;
  bool accessPointActive() const override;

  void onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) override;
  void onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) override;
  void onProvisionRequest(ProvisioningRequestHandler handler, void* context) override;

  void poll() override;

  // Built-in setup page (GET "/") + its POST "/apply" — no separate
  // WebServer, no loopback HTTP hop: "/apply" builds the {wifi,activation}
  // payload in-process and dispatches straight into the same /provision
  // handler. title=nullptr keeps the built-in default ("IoTLabs Device
  // Setup"); pass IOTLABS_PROJECT_NAME (or call this directly) to override.
  void setProvisioningPageTitle(const char* title) override;

  // When set (non-null, non-empty), the built-in setup page hides its
  // "Activation Token" field and "/apply" auto-fills submissions with this
  // value instead of requiring the user to type one in.
  void setDefaultActivationToken(const char* token) override;

  // When ssid is set (non-null, non-empty), the built-in setup page hides
  // its "WiFi SSID"/"WiFi Password" fields and "/apply" auto-fills
  // submissions with these values instead.
  void setDefaultWifiCredentials(const char* ssid, const char* password) override;

 private:
  WebServer _server;
  bool _active = false;
  bool _routesRegistered = false;
  const char* _pageTitle = nullptr;
  const char* _defaultActivationToken = nullptr;
  const char* _defaultWifiSsid = nullptr;
  const char* _defaultWifiPassword = nullptr;

  ProvisioningRequestHandler _deviceInfoHandler = nullptr;
  void* _deviceInfoContext = nullptr;
  ProvisioningRequestHandler _wifiNetworksHandler = nullptr;
  void* _wifiNetworksContext = nullptr;
  ProvisioningRequestHandler _provisionHandler = nullptr;
  void* _provisionContext = nullptr;

  void registerRoutesOnce();
  void dispatch(ProvisioningRequestHandler handler, void* context, bool withBody,
                const String* bodyOverride = nullptr);
  void handleNotFound();
  void handleProvisioningPage();
  void handleProvisioningPageApply();
  static String jsonEscape(const String& value);
};

#endif  // ARDUINO_ARCH_ESP32
