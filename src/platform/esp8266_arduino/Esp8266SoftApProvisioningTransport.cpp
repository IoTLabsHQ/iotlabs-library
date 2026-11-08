#include "Esp8266SoftApProvisioningTransport.h"

// Esp8266SoftApProvisioningTransport.h itself no-ops outside ESP8266 builds
// (it includes <ESP8266WebServer.h>), so guard this .cpp the same way.
#if defined(ARDUINO_ARCH_ESP8266)

#include <ESP8266WiFi.h>

Esp8266SoftApProvisioningTransport::Esp8266SoftApProvisioningTransport() : _server(80) {}

bool Esp8266SoftApProvisioningTransport::startAccessPoint(const char* apName, const char* apPassword) {
  if (_active) return true;

  WiFi.mode(WIFI_AP_STA);
  const bool ok = apPassword ? WiFi.softAP(apName, apPassword) : WiFi.softAP(apName);
  if (!ok) return false;

  registerRoutesOnce();
  _server.begin();
  _active = true;
  return true;
}

void Esp8266SoftApProvisioningTransport::stopAccessPoint() {
  if (!_active) return;
  _server.stop();
  WiFi.softAPdisconnect(true);
  _active = false;
  if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
}

bool Esp8266SoftApProvisioningTransport::accessPointActive() const { return _active; }

void Esp8266SoftApProvisioningTransport::onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) {
  _deviceInfoHandler = handler;
  _deviceInfoContext = context;
}

void Esp8266SoftApProvisioningTransport::onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) {
  _wifiNetworksHandler = handler;
  _wifiNetworksContext = context;
}

void Esp8266SoftApProvisioningTransport::onProvisionRequest(ProvisioningRequestHandler handler, void* context) {
  _provisionHandler = handler;
  _provisionContext = context;
}

void Esp8266SoftApProvisioningTransport::poll() { _server.handleClient(); }

void Esp8266SoftApProvisioningTransport::dispatch(ProvisioningRequestHandler handler, void* context, bool withBody) {
  if (!handler) {
    _server.send(500, "application/json", "{\"error\":{\"code\":\"NO_HANDLER\",\"message\":\"Route not wired\"}}");
    return;
  }

  const String body = withBody ? _server.arg("plain") : String();
  IoTLabsHttpExchange exchange;
  exchange.requestBody = body.c_str();

  handler(context, exchange);

  _server.send(exchange.responseStatus, exchange.responseContentType, exchange.responseBody);
}

void Esp8266SoftApProvisioningTransport::handleNotFound() {
  _server.send(404, "application/json", "{\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Unknown provisioning endpoint\"}}");
}

void Esp8266SoftApProvisioningTransport::registerRoutesOnce() {
  if (_routesRegistered) return;
  _routesRegistered = true;

  _server.on("/device-info", HTTP_GET, [this]() { dispatch(_deviceInfoHandler, _deviceInfoContext, false); });
  _server.on("/wifi/networks", HTTP_GET, [this]() { dispatch(_wifiNetworksHandler, _wifiNetworksContext, false); });
  _server.on("/provision", HTTP_POST, [this]() { dispatch(_provisionHandler, _provisionContext, true); });
  _server.onNotFound([this]() { handleNotFound(); });
}

#endif  // ARDUINO_ARCH_ESP8266
