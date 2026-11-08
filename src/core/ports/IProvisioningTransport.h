#pragma once

// Local provisioning HTTP surface. The transport only shuttles raw request
// bodies and raw response bodies over whatever local server the platform
// offers (SoftAP + WebServer, etc.); JSON parsing/building stays in core
// (ProvisioningManager) so ArduinoJson usage doesn't spread into adapters.
struct IoTLabsHttpExchange {
  const char* requestBody = "";
  int responseStatus = 200;
  const char* responseBody = "";
  const char* responseContentType = "application/json";
};

using ProvisioningRequestHandler = void (*)(void* context, IoTLabsHttpExchange& exchange);

class IProvisioningTransport {
 public:
  virtual ~IProvisioningTransport() = default;

  virtual bool startAccessPoint(const char* apName, const char* apPassword) = 0;
  virtual void stopAccessPoint() = 0;
  virtual bool accessPointActive() const = 0;

  virtual void onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) = 0;
  virtual void onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) = 0;
  virtual void onProvisionRequest(ProvisioningRequestHandler handler, void* context) = 0;

  virtual void poll() = 0;

  // Optional capability, not part of the core provisioning contract: a
  // transport that can render a human-facing setup page (SoftAP + HTML)
  // may use this to title it. Default no-op so platforms with no such
  // concept (Linux Unix-socket transport, STM32) need not implement it.
  virtual void setProvisioningPageTitle(const char* title) {}

  // Optional capability, mirrors setProvisioningPageTitle: a transport with
  // a human-facing setup page may use this to skip asking for the
  // activation token in its UI and auto-fill submissions with this value
  // instead. Default no-op so platforms with no such page need not
  // implement it.
  virtual void setDefaultActivationToken(const char* token) {}

  // Optional, mirrors setDefaultActivationToken: a transport with a
  // human-facing setup page may use this to skip asking for WiFi SSID/
  // password too and auto-fill submissions with these values instead.
  // password may legitimately be null/empty (open network). Default no-op.
  virtual void setDefaultWifiCredentials(const char* ssid, const char* password) {}
};
