#include "Rp2040SoftApProvisioningTransport.h"

#if defined(ARDUINO_ARCH_RP2040)

#include <WiFi.h>

Rp2040SoftApProvisioningTransport::Rp2040SoftApProvisioningTransport() : _server(80) {}

bool Rp2040SoftApProvisioningTransport::startAccessPoint(const char* apName, const char* apPassword) {
  if (_active) return true;

  WiFi.mode(WIFI_AP_STA);
  const bool ok = apPassword ? WiFi.softAP(apName, apPassword) : WiFi.softAP(apName);
  if (!ok) return false;

  registerRoutesOnce();
  _server.begin();
  _active = true;
  return true;
}

void Rp2040SoftApProvisioningTransport::stopAccessPoint() {
  if (!_active) return;
  _server.stop();
  WiFi.softAPdisconnect(true);
  _active = false;
  if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
}

bool Rp2040SoftApProvisioningTransport::accessPointActive() const { return _active; }

void Rp2040SoftApProvisioningTransport::onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) {
  _deviceInfoHandler = handler;
  _deviceInfoContext = context;
}

void Rp2040SoftApProvisioningTransport::onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) {
  _wifiNetworksHandler = handler;
  _wifiNetworksContext = context;
}

void Rp2040SoftApProvisioningTransport::onProvisionRequest(ProvisioningRequestHandler handler, void* context) {
  _provisionHandler = handler;
  _provisionContext = context;
}

void Rp2040SoftApProvisioningTransport::poll() { _server.handleClient(); }

void Rp2040SoftApProvisioningTransport::dispatch(ProvisioningRequestHandler handler, void* context, bool withBody) {
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

void Rp2040SoftApProvisioningTransport::handleNotFound() {
  _server.send(404, "application/json", "{\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Unknown provisioning endpoint\"}}");
}

void Rp2040SoftApProvisioningTransport::registerRoutesOnce() {
  if (_routesRegistered) return;
  _routesRegistered = true;

  _server.on("/device-info", HTTP_GET, [this]() { dispatch(_deviceInfoHandler, _deviceInfoContext, false); });
  _server.on("/wifi/networks", HTTP_GET, [this]() { dispatch(_wifiNetworksHandler, _wifiNetworksContext, false); });
  _server.on("/provision", HTTP_POST, [this]() { dispatch(_provisionHandler, _provisionContext, true); });
  _server.onNotFound([this]() { handleNotFound(); });
}

#endif  // ARDUINO_ARCH_RP2040
