#include "ProvisioningManager.h"

#include <ArduinoJson.h>

#include <cstring>

#include "IoTLabsRuntimeIdentity.h"

ProvisioningManager::ProvisioningManager(IProvisioningTransport& transport, INetwork& network,
                                          IStorage& storage, ISecretStore& secrets, ILogger* logger)
    : _transport(transport), _network(network), _storage(storage), _secrets(secrets), _logger(logger) {}

void ProvisioningManager::configure(const Config& config, const std::string& hardwareId) {
  _config = config;
  _hardwareId = hardwareId;

  // Suffix comes from the Device Platform deviceId (validateConfig()
  // requires it non-empty), not the local hardwareId: the server doesn't
  // learn hardwareId until the FIRST successful activation, so it could
  // never predict/display this SoftAP's SSID in Console beforehand.
  const std::string deviceIdStr(_config.deviceId ? _config.deviceId : "");
  const std::string suffix = deviceIdStr.size() >= 6 ? deviceIdStr.substr(deviceIdStr.size() - 6) : deviceIdStr;
  _apName = std::string(_config.provisioningApPrefix ? _config.provisioningApPrefix : "IoTLabs") + "-" + suffix;

  _transport.setProvisioningPageTitle(_config.provisioningPageTitle);
  _transport.setDefaultActivationToken(_config.activationSecret);
  _transport.setDefaultWifiCredentials(_config.wifiSsid, _config.wifiPassword);
}

void ProvisioningManager::setEventCallback(EventCallback callback, void* context) {
  _eventCallback = callback;
  _eventContext = context;
}

void ProvisioningManager::setAcceptedCallback(AcceptedCallback callback, void* context) {
  _acceptedCallback = callback;
  _acceptedContext = context;
}

void ProvisioningManager::emit(IoTLabsEvent event) {
  if (_eventCallback) _eventCallback(_eventContext, event);
}

void ProvisioningManager::registerRoutes() {
  if (_routesRegistered) return;
  _routesRegistered = true;
  _transport.onDeviceInfoRequest(&ProvisioningManager::deviceInfoThunk, this);
  _transport.onWifiNetworksRequest(&ProvisioningManager::wifiNetworksThunk, this);
  _transport.onProvisionRequest(&ProvisioningManager::provisionThunk, this);
}

bool ProvisioningManager::start() {
  if (_active) return true;

  const char* password = _config.provisioningApPassword;
  const bool hasPassword = password && strlen(password) >= 8;
  if (!hasPassword) {
    iotlabs_log::line(_logger, "[IoTLabs] WARNING: provisioning SoftAP is open; development use only");
  }

  if (!_transport.startAccessPoint(_apName.c_str(), hasPassword ? password : nullptr)) {
    return false;
  }

  registerRoutes();
  _active = true;

  iotlabs_log::line(_logger, "[IoTLabs] Provisioning AP: %s", _apName.c_str());
  emit(IoTLabsEvent::PROVISIONING_STARTED);
  return true;
}

void ProvisioningManager::stop() {
  if (!_active) return;
  _transport.stopAccessPoint();
  _active = false;
}

void ProvisioningManager::poll() {
  if (_active) _transport.poll();
}

void ProvisioningManager::sendError(IoTLabsHttpExchange& exchange, int status, const char* code, const char* message) {
  JsonDocument doc;
  doc["error"]["code"] = code;
  doc["error"]["message"] = message;
  _responseScratch.clear();
  serializeJson(doc, _responseScratch);
  exchange.responseStatus = status;
  exchange.responseBody = _responseScratch.c_str();
}

void ProvisioningManager::handleDeviceInfo(IoTLabsHttpExchange& exchange) {
  IoTLabsRuntimeIdentity identity;
  const bool provisioned = _storage.loadRuntimeIdentity(identity) && identity.valid();

  JsonDocument doc;
  doc["platform"] = "esp32";
  doc["hardwareId"] = _hardwareId;
  doc["deviceTemplateId"] = _config.deviceTemplateId;
  doc["deviceTemplateVersion"] = _config.deviceTemplateVersion;
  doc["firmwareVersion"] = _config.firmwareVersion;
  doc["sdkVersion"] = IOTLABS_SDK_VERSION;
  doc["protocolVersion"] = IOTLABS_PROTOCOL_VERSION;
  doc["provisioningVersion"] = IOTLABS_PROVISIONING_VERSION;
  doc["provisioned"] = provisioned;
  if (provisioned) doc["deviceId"] = identity.deviceId;

  _responseScratch.clear();
  serializeJson(doc, _responseScratch);
  exchange.responseStatus = 200;
  exchange.responseBody = _responseScratch.c_str();
}

void ProvisioningManager::handleWifiNetworks(IoTLabsHttpExchange& exchange) {
  static constexpr int kMaxResults = 20;
  IoTLabsNetworkScanResult results[kMaxResults];
  const int count = _network.scanNetworks(results, kMaxResults);

  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int i = 0; i < count; ++i) {
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = results[i].ssid;
    item["rssi"] = results[i].rssi;
    item["secure"] = results[i].secure;
  }

  _responseScratch.clear();
  serializeJson(doc, _responseScratch);
  exchange.responseStatus = 200;
  exchange.responseBody = _responseScratch.c_str();
}

void ProvisioningManager::handleProvision(IoTLabsHttpExchange& exchange) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, exchange.requestBody);
  if (error) {
    sendError(exchange, 400, "INVALID_REQUEST", "Invalid JSON payload");
    return;
  }

  const char* ssid = doc["wifi"]["ssid"] | "";
  const char* password = doc["wifi"]["password"] | "";
  const char* token = doc["activation"]["token"] | "";

  if (strlen(ssid) == 0) {
    sendError(exchange, 422, "WIFI_SSID_REQUIRED", "wifi.ssid is required");
    return;
  }

  IoTLabsRuntimeIdentity identity;
  const bool alreadyProvisioned = _storage.loadRuntimeIdentity(identity) && identity.valid();

  if (!alreadyProvisioned && strlen(token) == 0) {
    sendError(exchange, 422, "ACTIVATION_TOKEN_REQUIRED", "activation.token is required for an unprovisioned device");
    return;
  }

  if (!_storage.saveWifiSsid(ssid) || !_secrets.saveWifiPassword(password)) {
    sendError(exchange, 500, "STORAGE_WRITE_FAILED", "Could not store WiFi configuration");
    return;
  }

  const bool hasNewToken = strlen(token) > 0;
  if (hasNewToken) _secrets.saveActivationToken(token);

  JsonDocument response;
  response["accepted"] = true;
  response["mode"] = alreadyProvisioned ? "wifi_reconfigure" : "provision";
  _responseScratch.clear();
  serializeJson(response, _responseScratch);
  exchange.responseStatus = 202;
  exchange.responseBody = _responseScratch.c_str();

  emit(IoTLabsEvent::PROVISIONING_ACCEPTED);
  if (_acceptedCallback) _acceptedCallback(_acceptedContext, hasNewToken);
}

bool ProvisioningManager::tryAutoProvision() {
  if (!_config.wifiSsid || strlen(_config.wifiSsid) == 0) return false;
  if (!_config.activationSecret || strlen(_config.activationSecret) == 0) return false;

  JsonDocument doc;
  doc["wifi"]["ssid"] = _config.wifiSsid;
  doc["wifi"]["password"] = _config.wifiPassword ? _config.wifiPassword : "";
  doc["activation"]["token"] = _config.activationSecret;

  std::string body;
  serializeJson(doc, body);

  IoTLabsHttpExchange exchange;
  exchange.requestBody = body.c_str();
  handleProvision(exchange);
  return exchange.responseStatus == 202;
}

void ProvisioningManager::deviceInfoThunk(void* context, IoTLabsHttpExchange& exchange) {
  static_cast<ProvisioningManager*>(context)->handleDeviceInfo(exchange);
}

void ProvisioningManager::wifiNetworksThunk(void* context, IoTLabsHttpExchange& exchange) {
  static_cast<ProvisioningManager*>(context)->handleWifiNetworks(exchange);
}

void ProvisioningManager::provisionThunk(void* context, IoTLabsHttpExchange& exchange) {
  static_cast<ProvisioningManager*>(context)->handleProvision(exchange);
}
