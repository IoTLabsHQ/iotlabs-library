#pragma once

#include <cstdint>
#include <string>

#include "../IoTLabsTypes.h"
#include "../IoTLabsValue.h"
#include "ActivationService.h"
#include "AutomationEngine.h"
#include "ConnectionManager.h"
#include "ControlRouter.h"
#include "Dependencies.h"
#include "IoTLabsRuntimeIdentity.h"
#include "ProvisioningManager.h"
#include "RuntimeManager.h"
#include "StateRegistry.h"

// The portable "IoTLabs Facade" internals (IoTLabs internal design notes):
// orchestrates ControlRouter/StateRegistry/ConnectionManager/ProvisioningManager/
// ActivationService/RuntimeManager purely through ports, with zero Arduino
// networking/MQTT/storage headers. The public IoTLabs class (src/IoTLabs.h)
// is a thin PIMPL wrapper around this engine.
class IoTLabsEngine {
 public:
  using EventHandler = void (*)(IoTLabsEvent);
  using CommandFinishHandler = void (*)(const char* controlId);

  explicit IoTLabsEngine(const IoTLabsDependencies& deps);

  bool begin(const IoTLabsConfig& config);
  void loop();

  bool onBool(const char* controlId, ControlRouter::BoolHandler handler);
  bool onNumber(const char* controlId, ControlRouter::NumberHandler handler);
  bool onString(const char* controlId, ControlRouter::StringHandler handler);
  bool onJson(const char* controlId, ControlRouter::JsonHandler handler);

  bool reportBool(const char* controlId, bool value, bool publishNow);
  bool reportInt(const char* controlId, long value, bool publishNow);
  bool reportNumber(const char* controlId, double value, bool publishNow);
  bool reportString(const char* controlId, const char* value, bool publishNow);
  bool reportJson(const char* controlId, JsonVariantConst value, bool publishNow);

  bool connected() const;
  bool wifiConnected() const;
  bool provisioned() const;
  bool provisioning() const;
  IoTLabsStatus status() const { return _status; }

  void onEvent(EventHandler handler) { _eventHandler = handler; }
  void onCommandFinish(CommandFinishHandler handler) { _commandFinishHandler = handler; }

  bool startProvisioning();
  bool resetWiFi();
  bool resetIdentity();
  bool factoryReset(bool restartDevice);

  bool publishTelemetrySnapshot();
  bool publishStatus();

  const char* hardwareId() const { return _hardwareId.c_str(); }
  const char* deviceId() const { return _identity.deviceId.c_str(); }
  const char* deviceTemplateId() const { return _config.deviceTemplateId ? _config.deviceTemplateId : ""; }
  uint32_t deviceTemplateVersion() const { return _config.deviceTemplateVersion; }
  const char* provisioningApName() const { return _provisioning.apName().c_str(); }
  const char* lastError() const { return _lastError.c_str(); }

  int mqttState() const { return _runtime.lastState(); }
  uint32_t reconnectDelayMs() const;

 private:
  IoTLabsDependencies _deps;
  IoTLabsConfig _config;

  ControlRouter _controls;
  StateRegistry _states;
  AutomationEngine _automation;
  ConnectionManager _connection;
  ProvisioningManager _provisioning;
  ActivationService _activation;
  RuntimeManager _runtime;

  IoTLabsRuntimeIdentity _identity;
  std::string _hardwareId;
  std::string _lastError;

  IoTLabsStatus _status = IoTLabsStatus::BOOT;
  bool _started = false;
  // Set once storage->begin() succeeds inside begin() — narrower than
  // _started (which is only set after the STORED_PROFILE_MISMATCH check),
  // so resetIdentity() stays usable even when begin() bails out on that
  // check with stale persisted identity from a prior Profile.
  bool _storageReady = false;
  EventHandler _eventHandler = nullptr;
  CommandFinishHandler _commandFinishHandler = nullptr;

  bool validateConfig();
  void generateHardwareId();
  void loadPersistentState();
  const char* resolvedMqttRootCA() const;
  bool startProvisioningInternal();

  void setError(const std::string& code);
  void handleManagerEvent(IoTLabsEvent event);
  void handleProvisioningAccepted(bool hasNewToken);
  void handleManagerCommandFinish(const char* controlId);

  // Shared by begin() and loop(): begin() kicks off the first Wi-Fi attempt
  // synchronously (matching the original monolith's startWifiAttempt() call
  // inside begin()), so both call sites must run the exact same
  // event-synthesis logic instead of begin() silently discarding its tick.
  // Returns true if the caller should stop servicing this tick (not
  // wifi-connected yet).
  bool handleConnectionResult(const ConnectionManager::TickResult& result, uint32_t now);

  static void onManagerEvent(void* context, IoTLabsEvent event);
  static void onProvisioningAccepted(void* context, bool hasNewToken);
  static void onManagerCommandFinish(void* context, const char* controlId);
};
