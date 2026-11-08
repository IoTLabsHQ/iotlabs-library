#pragma once

#include <string>

#include "../IoTLabsTypes.h"
#include "Log.h"
#include "ports/INetwork.h"
#include "ports/IProvisioningTransport.h"
#include "ports/ISecretStore.h"
#include "ports/IStorage.h"

// Local SoftAP + HTTP provisioning surface (IoTLabs internal design notes).
// Owns the /device-info, /wifi/networks and /provision routes end to end;
// the transport port only carries raw bytes so ArduinoJson usage stays here.
class ProvisioningManager {
 public:
  struct Config {
    const char* deviceTemplateId = nullptr;
    uint32_t deviceTemplateVersion = 1;
    const char* firmwareVersion = nullptr;
    const char* provisioningApPrefix = "IoTLabs";
    const char* provisioningApPassword = nullptr;
    const char* provisioningPageTitle = nullptr;
    // Required (IoTLabsEngine::validateConfig() enforces it) -- see
    // IoTLabsConfig::deviceId. Drives the SoftAP SSID suffix below instead
    // of the local hardwareId.
    const char* deviceId = nullptr;
    // Optional -- see IoTLabsConfig::activationSecret.
    const char* activationSecret = nullptr;
    // Optional -- see IoTLabsConfig::wifiSsid/wifiPassword.
    const char* wifiSsid = nullptr;
    const char* wifiPassword = nullptr;
  };

  using EventCallback = void (*)(void* context, IoTLabsEvent event);
  // hasNewToken: a fresh activation token was accepted, so the caller should
  // also reset activation backoff and retry immediately.
  using AcceptedCallback = void (*)(void* context, bool hasNewToken);

  ProvisioningManager(IProvisioningTransport& transport, INetwork& network,
                       IStorage& storage, ISecretStore& secrets, ILogger* logger);

  void configure(const Config& config, const std::string& hardwareId);
  void setEventCallback(EventCallback callback, void* context);
  void setAcceptedCallback(AcceptedCallback callback, void* context);

  bool start();
  void stop();

  // When both wifiSsid and activationSecret are compiled in (see
  // IoTLabsConfig), runs the exact same validation/persistence path a real
  // SoftAP "/provision" POST would (handleProvision()), just built from
  // compiled-in values instead of an HTTP request body -- so a fresh device
  // can connect + activate with zero manual/captive-portal interaction.
  // No-op (returns false) if either value is missing; safe to call
  // unconditionally. Does not prevent the SoftAP from also starting (AP+STA
  // coexist) -- it stays available as a fallback if activation fails.
  bool tryAutoProvision();
  bool active() const { return _active; }
  const std::string& apName() const { return _apName; }
  void poll();

 private:
  IProvisioningTransport& _transport;
  INetwork& _network;
  IStorage& _storage;
  ISecretStore& _secrets;
  ILogger* _logger;

  Config _config;
  std::string _hardwareId;
  std::string _apName;
  bool _active = false;
  bool _routesRegistered = false;

  EventCallback _eventCallback = nullptr;
  void* _eventContext = nullptr;
  AcceptedCallback _acceptedCallback = nullptr;
  void* _acceptedContext = nullptr;

  std::string _responseScratch;

  void emit(IoTLabsEvent event);
  void registerRoutes();

  void handleDeviceInfo(IoTLabsHttpExchange& exchange);
  void handleWifiNetworks(IoTLabsHttpExchange& exchange);
  void handleProvision(IoTLabsHttpExchange& exchange);
  void sendError(IoTLabsHttpExchange& exchange, int status, const char* code, const char* message);

  static void deviceInfoThunk(void* context, IoTLabsHttpExchange& exchange);
  static void wifiNetworksThunk(void* context, IoTLabsHttpExchange& exchange);
  static void provisionThunk(void* context, IoTLabsHttpExchange& exchange);
};
