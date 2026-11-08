#include "IoTLabsEngine.h"

#include <cstring>

#include "Log.h"

IoTLabsEngine::IoTLabsEngine(const IoTLabsDependencies& deps)
    : _deps(deps),
      _connection(*deps.network, *deps.clock, deps.logger),
      _provisioning(*deps.provisioning, *deps.network, *deps.storage, *deps.secrets, deps.logger),
      _activation(*deps.http, *deps.clock, deps.logger),
      _runtime(*deps.mqtt, *deps.clock, _controls, _states, _automation, deps.logger) {}

bool IoTLabsEngine::validateConfig() {
  if (!_config.deviceTemplateId || strlen(_config.deviceTemplateId) == 0) {
    setError("DEVICE_TEMPLATE_ID_REQUIRED");
    return false;
  }
  if (!_config.firmwareVersion || strlen(_config.firmwareVersion) == 0) {
    setError("FIRMWARE_VERSION_REQUIRED");
    return false;
  }
  if (!_config.provisioningUrl || strlen(_config.provisioningUrl) == 0) {
    setError("PROVISIONING_URL_REQUIRED");
    return false;
  }
  if (!_config.deviceId || strlen(_config.deviceId) == 0) {
    setError("DEVICE_ID_REQUIRED");
    return false;
  }
  return true;
}

void IoTLabsEngine::generateHardwareId() {
  char buffer[64] = {0};
  _deps.identity->readHardwareId(buffer, sizeof(buffer));
  _hardwareId = buffer;
}

void IoTLabsEngine::loadPersistentState() {
  std::string ssid, password, token;
  _deps.storage->loadWifiSsid(ssid);
  _deps.secrets->loadWifiPassword(password);
  _deps.secrets->loadActivationToken(token);
  _deps.storage->loadRuntimeIdentity(_identity);

  _connection.setCredentials(ssid, password);
  _activation.setToken(token);
}

const char* IoTLabsEngine::resolvedMqttRootCA() const {
  if (_config.mqttRootCA && strlen(_config.mqttRootCA) > 0) return _config.mqttRootCA;
  return _config.provisioningRootCA;
}

bool IoTLabsEngine::startProvisioningInternal() {
  const bool ok = _provisioning.start();
  if (!ok) setError("PROVISIONING_AP_FAILED");
  return ok;
}

void IoTLabsEngine::setError(const std::string& code) {
  _lastError = code;
  iotlabs_log::line(_deps.logger, "[IoTLabs] ERROR: %s", code.c_str());
  handleManagerEvent(IoTLabsEvent::ERROR);
}

void IoTLabsEngine::handleManagerEvent(IoTLabsEvent event) {
  switch (event) {
    case IoTLabsEvent::BOOT:
      _status = IoTLabsStatus::BOOT;
      break;
    case IoTLabsEvent::WIFI_CONNECTING:
      _status = IoTLabsStatus::WIFI_CONNECTING;
      break;
    case IoTLabsEvent::ACTIVATING:
      _status = IoTLabsStatus::ACTIVATING;
      break;
    case IoTLabsEvent::CLOUD_CONNECTING:
      _status = IoTLabsStatus::CLOUD_CONNECTING;
      break;
    case IoTLabsEvent::ONLINE:
      _status = IoTLabsStatus::ONLINE;
      break;
    case IoTLabsEvent::OFFLINE:
      _status = IoTLabsStatus::OFFLINE;
      break;
    case IoTLabsEvent::ERROR:
      _status = IoTLabsStatus::ERROR;
      break;
    case IoTLabsEvent::PROVISIONING_STARTED:
      _status = _identity.valid() ? IoTLabsStatus::PROVISIONING : IoTLabsStatus::UNPROVISIONED;
      break;
    default:
      break;
  }
  if (_eventHandler) _eventHandler(event);
}

void IoTLabsEngine::handleProvisioningAccepted(bool hasNewToken) {
  std::string ssid, password;
  _deps.storage->loadWifiSsid(ssid);
  _deps.secrets->loadWifiPassword(password);

  const uint32_t now = _deps.clock->monotonicMillis();
  _connection.applyNewCredentials(ssid, password, now + 100);

  if (hasNewToken) {
    std::string token;
    _deps.secrets->loadActivationToken(token);
    _activation.setToken(token);
    _activation.requestImmediateAttempt(now);
  }
}

void IoTLabsEngine::handleManagerCommandFinish(const char* controlId) {
  if (_commandFinishHandler) _commandFinishHandler(controlId);
}

void IoTLabsEngine::onManagerEvent(void* context, IoTLabsEvent event) {
  static_cast<IoTLabsEngine*>(context)->handleManagerEvent(event);
}

void IoTLabsEngine::onProvisioningAccepted(void* context, bool hasNewToken) {
  static_cast<IoTLabsEngine*>(context)->handleProvisioningAccepted(hasNewToken);
}

void IoTLabsEngine::onManagerCommandFinish(void* context, const char* controlId) {
  static_cast<IoTLabsEngine*>(context)->handleManagerCommandFinish(controlId);
}

// ============================================================
// Lifecycle
// ============================================================

bool IoTLabsEngine::begin(const IoTLabsConfig& config) {
  if (_started) return true;

  _config = config;
  handleManagerEvent(IoTLabsEvent::BOOT);

  if (!validateConfig()) return false;
  if (!_deps.storage->begin(config.storageNamespace)) {
    setError("STORAGE_INIT_FAILED");
    return false;
  }
  _storageReady = true;

  generateHardwareId();
  _automation.begin(*_deps.storage);

  ConnectionManager::Config connConfig;
  connConfig.reconnectMinMs = config.reconnectMinMs;
  connConfig.reconnectMaxMs = config.reconnectMaxMs;
  connConfig.wifiConnectTimeoutMs = config.wifiConnectTimeoutMs;
  _connection.configure(connConfig);

  ProvisioningManager::Config provConfig;
  provConfig.deviceTemplateId = config.deviceTemplateId;
  provConfig.deviceTemplateVersion = config.deviceTemplateVersion;
  provConfig.firmwareVersion = config.firmwareVersion;
  provConfig.provisioningApPrefix = config.provisioningApPrefix;
  provConfig.provisioningApPassword = config.provisioningApPassword;
  provConfig.provisioningPageTitle = config.provisioningPageTitle;
  provConfig.deviceId = config.deviceId;
  provConfig.activationSecret = config.activationSecret;
  provConfig.wifiSsid = config.wifiSsid;
  provConfig.wifiPassword = config.wifiPassword;
  _provisioning.configure(provConfig, _hardwareId);
  _provisioning.setEventCallback(&IoTLabsEngine::onManagerEvent, this);
  _provisioning.setAcceptedCallback(&IoTLabsEngine::onProvisioningAccepted, this);

  ActivationService::Config actConfig;
  actConfig.deviceTemplateId = config.deviceTemplateId;
  actConfig.deviceTemplateVersion = config.deviceTemplateVersion;
  actConfig.firmwareVersion = config.firmwareVersion;
  actConfig.provisioningUrl = config.provisioningUrl;
  actConfig.provisioningPath = config.provisioningPath;
  actConfig.tlsInsecure = config.tlsInsecure;
  actConfig.provisioningRootCA = config.provisioningRootCA;
  actConfig.httpTimeoutMs = config.activationHttpTimeoutMs;
  actConfig.retryMinMs = config.activationRetryMinMs;
  actConfig.retryMaxMs = config.activationRetryMaxMs;
  _activation.configure(actConfig, _hardwareId);
  _activation.setEventCallback(&IoTLabsEngine::onManagerEvent, this);

  RuntimeManager::StaticConfig runtimeConfig;
  runtimeConfig.deviceTemplateId = config.deviceTemplateId;
  runtimeConfig.deviceTemplateVersion = config.deviceTemplateVersion;
  runtimeConfig.firmwareVersion = config.firmwareVersion;
  runtimeConfig.tlsInsecure = config.tlsInsecure;
  runtimeConfig.reconnectMinMs = config.reconnectMinMs;
  runtimeConfig.reconnectMaxMs = config.reconnectMaxMs;
  runtimeConfig.telemetryIntervalMs = config.telemetryIntervalMs;
  runtimeConfig.statusIntervalMs = config.statusIntervalMs;
  runtimeConfig.keepAliveSeconds = config.keepAliveSeconds;
  runtimeConfig.socketTimeoutSeconds = config.socketTimeoutSeconds;
  runtimeConfig.mqttBufferBytes = config.mqttBufferBytes;
  _runtime.configureStatic(runtimeConfig, _hardwareId);
  _runtime.setEventCallback(&IoTLabsEngine::onManagerEvent, this);
  _runtime.setCommandFinishCallback(&IoTLabsEngine::onManagerCommandFinish, this);

  // Managers are fully configured before we pull in persisted state, so any
  // side effects triggered while loading it (e.g. activation backoff reset)
  // use the real config instead of a struct default.
  loadPersistentState();

  // IOTLABS_WIFI_SSID + IOTLABS_DEVICE_ACTIVATION_SECRET compiled in
  // together on a fresh/unprovisioned device let it connect + activate
  // with zero manual interaction -- same validated path a real SoftAP
  // "/provision" POST takes (see ProvisioningManager::tryAutoProvision()),
  // it just persists synchronously here instead of waiting for one. A no-op
  // if the device is already provisioned or already has WiFi credentials
  // (e.g. a prior manual provision, or a previous auto-provision run).
  if (!_identity.valid() && !_connection.hasCredentials()) {
    _provisioning.tryAutoProvision();
  }

  if (_identity.valid() && !_identity.deviceTemplateId.empty() &&
      _identity.deviceTemplateId != std::string(_config.deviceTemplateId ? _config.deviceTemplateId : "")) {
    setError("STORED_DEVICE_TEMPLATE_MISMATCH");
    return false;
  }

  _started = true;

  iotlabs_log::line(_deps.logger, "[IoTLabs] SDK %s", IOTLABS_SDK_VERSION);
  iotlabs_log::line(_deps.logger, "[IoTLabs] Hardware ID: %s", _hardwareId.c_str());
  iotlabs_log::line(_deps.logger, "[IoTLabs] Device Template: %s v%u", config.deviceTemplateId,
                     static_cast<unsigned>(config.deviceTemplateVersion));

  const uint32_t now = _deps.clock->monotonicMillis();
  _connection.requestImmediateAttempt(now);
  _activation.requestImmediateAttempt(now);

  if (_identity.valid()) {
    if (!_runtime.configureIdentity(_identity, resolvedMqttRootCA())) {
      setError("MQTT_ROOT_CA_REQUIRED");
      return false;
    }
    if (!_connection.hasCredentials()) {
      iotlabs_log::line(_deps.logger, "[IoTLabs] Device identity exists but WiFi is not configured");
      startProvisioningInternal();
    } else {
      handleConnectionResult(_connection.tick(now), now);
    }
    return true;
  }

  startProvisioningInternal();
  if (_connection.hasCredentials()) handleConnectionResult(_connection.tick(now), now);
  return true;
}

bool IoTLabsEngine::handleConnectionResult(const ConnectionManager::TickResult& connResult, uint32_t now) {
  if (connResult.attemptStarted) handleManagerEvent(IoTLabsEvent::WIFI_CONNECTING);

  if (connResult.justConnected) {
    handleManagerEvent(IoTLabsEvent::WIFI_CONNECTED);
    if (_identity.valid() && _provisioning.active()) _provisioning.stop();
    _activation.requestImmediateAttempt(now);
    _runtime.requestImmediateAttempt(now);
  }

  if (connResult.justDisconnected) {
    _runtime.forceDisconnect();
    handleManagerEvent(IoTLabsEvent::OFFLINE);
  }

  if (connResult.connectTimedOut) {
    handleManagerEvent(IoTLabsEvent::WIFI_FAILED);
    if (!_identity.valid() && !_provisioning.active()) startProvisioningInternal();
  }

  if (connResult.noCredentials) {
    if (!_provisioning.active()) startProvisioningInternal();
    return true;
  }

  return !_connection.connected();
}

void IoTLabsEngine::loop() {
  if (!_started) return;
  _provisioning.poll();

  const uint32_t now = _deps.clock->monotonicMillis();
  if (handleConnectionResult(_connection.tick(now), now)) return;

  if (!_identity.valid()) {
    if (!_activation.hasToken()) {
      _status = IoTLabsStatus::PROVISIONING;
      if (!_provisioning.active()) startProvisioningInternal();
      return;
    }

    IoTLabsRuntimeIdentity newIdentity;
    std::string errorCode;
    const ActivationService::Outcome outcome = _activation.tick(now, newIdentity, errorCode);

    if (outcome == ActivationService::Outcome::SUCCESS) {
      if (!_deps.storage->saveRuntimeIdentity(newIdentity)) {
        _activation.forceBlocked("RUNTIME_IDENTITY_STORAGE_FAILED");
        setError("RUNTIME_IDENTITY_STORAGE_FAILED");
        return;
      }

      _identity = newIdentity;
      _deps.secrets->clearActivationToken();
      _activation.clearToken();
      _lastError.clear();
      iotlabs_log::line(_deps.logger, "[IoTLabs] Activated Device ID: %s", _identity.deviceId.c_str());

      if (!_runtime.configureIdentity(_identity, resolvedMqttRootCA())) {
        setError("MQTT_ROOT_CA_REQUIRED");
        return;
      }
      _runtime.requestImmediateAttempt(now);
      if (_provisioning.active()) _provisioning.stop();
      handleManagerEvent(IoTLabsEvent::ACTIVATED);
    } else if (outcome == ActivationService::Outcome::BLOCKED) {
      setError(errorCode);
      if (!_provisioning.active()) startProvisioningInternal();
    }
    return;
  }

  // Already provisioned. A fresh activation token can still arrive here --
  // ProvisioningManager accepts one via /provision even when alreadyProvisioned
  // (see handleProvision()), e.g. after a backend-side credential rotation
  // (reprovision session -> new token delivered locally). Without this,
  // ActivationService held a token that nothing ever consumed, since the
  // branch above only runs while !_identity.valid(). Keep servicing the
  // existing (still-working) runtime connection throughout; only swap
  // identity once a rotation attempt actually succeeds, and never drop the
  // working connection just because a rotation attempt is blocked/retrying.
  if (_activation.hasToken()) {
    IoTLabsRuntimeIdentity rotatedIdentity;
    std::string errorCode;
    const ActivationService::Outcome outcome = _activation.tick(now, rotatedIdentity, errorCode);

    if (outcome == ActivationService::Outcome::SUCCESS) {
      if (!_deps.storage->saveRuntimeIdentity(rotatedIdentity)) {
        _activation.forceBlocked("RUNTIME_IDENTITY_STORAGE_FAILED");
        setError("RUNTIME_IDENTITY_STORAGE_FAILED");
      } else {
        _identity = rotatedIdentity;
        _deps.secrets->clearActivationToken();
        _activation.clearToken();
        _lastError.clear();
        iotlabs_log::line(_deps.logger, "[IoTLabs] Rotated Device ID: %s", _identity.deviceId.c_str());

        if (!_runtime.configureIdentity(_identity, resolvedMqttRootCA())) {
          setError("MQTT_ROOT_CA_REQUIRED");
        } else {
          // Old credentials are no longer valid once the backend rotates
          // them -- drop the current session so the fall-through tick()
          // below reconnects immediately with the new ones.
          _runtime.forceDisconnect();
          _runtime.requestImmediateAttempt(now);
          handleManagerEvent(IoTLabsEvent::ACTIVATED);
        }
      }
    } else if (outcome == ActivationService::Outcome::BLOCKED) {
      setError(errorCode);
    }
    // NOT_DUE / RETRY_SCHEDULED: nothing to do this tick; keep running below.
  }

  _runtime.tick(now);
  // Evaluated every tick regardless of cloud connectivity — see
  // AutomationEngine.h for why this stays independent of _runtime's state.
  _automation.evaluate(_states, _controls);
}

// ============================================================
// Controls / reported state
// ============================================================

bool IoTLabsEngine::onBool(const char* controlId, ControlRouter::BoolHandler handler) {
  return _controls.on(controlId, handler);
}
bool IoTLabsEngine::onNumber(const char* controlId, ControlRouter::NumberHandler handler) {
  return _controls.on(controlId, handler);
}
bool IoTLabsEngine::onString(const char* controlId, ControlRouter::StringHandler handler) {
  return _controls.on(controlId, handler);
}
bool IoTLabsEngine::onJson(const char* controlId, ControlRouter::JsonHandler handler) {
  return _controls.on(controlId, handler);
}

bool IoTLabsEngine::reportBool(const char* controlId, bool value, bool publishNow) {
  if (!_states.reportBool(controlId, value)) return false;
  if (!publishNow || !_runtime.connected()) return true;
  return _runtime.publishSingleState(controlId);
}

bool IoTLabsEngine::reportInt(const char* controlId, long value, bool publishNow) {
  if (!_states.reportInt(controlId, value)) return false;
  if (!publishNow || !_runtime.connected()) return true;
  return _runtime.publishSingleState(controlId);
}

bool IoTLabsEngine::reportNumber(const char* controlId, double value, bool publishNow) {
  if (!_states.reportNumber(controlId, value)) return false;
  if (!publishNow || !_runtime.connected()) return true;
  return _runtime.publishSingleState(controlId);
}

bool IoTLabsEngine::reportString(const char* controlId, const char* value, bool publishNow) {
  if (!_states.reportString(controlId, value ? value : "")) return false;
  if (!publishNow || !_runtime.connected()) return true;
  return _runtime.publishSingleState(controlId);
}

bool IoTLabsEngine::reportJson(const char* controlId, JsonVariantConst value, bool publishNow) {
  if (!_states.reportJson(controlId, value)) return false;
  if (!publishNow || !_runtime.connected()) return true;
  return _runtime.publishSingleState(controlId);
}

// ============================================================
// Status / lifecycle helpers
// ============================================================

bool IoTLabsEngine::connected() const { return _runtime.connected(); }
bool IoTLabsEngine::wifiConnected() const { return _connection.connected(); }
bool IoTLabsEngine::provisioned() const { return _identity.valid(); }
bool IoTLabsEngine::provisioning() const { return _provisioning.active() && !_identity.valid(); }

bool IoTLabsEngine::startProvisioning() {
  if (!_started) return false;
  return startProvisioningInternal();
}

bool IoTLabsEngine::resetWiFi() {
  if (!_started) return false;
  _deps.storage->clearWifi();
  _deps.secrets->clearWifiPassword();
  _connection.clearCredentials();
  _deps.network->disconnect(true);
  return startProvisioningInternal();
}

// Clears stored deviceId/deviceTemplateId/MQTT-credential identity + the
// pending activation token, so a physical board can be re-provisioned for a
// DIFFERENT Device/DeviceTemplate (dev/test reflash workflow) without a full
// factoryReset() wiping WiFi credentials and automation rules too. Gated
// on _storageReady (not _started) so it stays callable right after a
// begin() that bailed out on STORED_DEVICE_TEMPLATE_MISMATCH — storage/
// provisioning/activation are already initialized by that point.
bool IoTLabsEngine::resetIdentity() {
  if (!_storageReady) return false;
  _runtime.forceDisconnect();
  _deps.storage->clearRuntimeIdentity();
  _deps.secrets->clearActivationToken();
  _identity = IoTLabsRuntimeIdentity();
  _activation.clearToken();
  _activation.unblock();
  return startProvisioningInternal();
}

bool IoTLabsEngine::factoryReset(bool restartDevice) {
  if (!_started) return false;
  _runtime.forceDisconnect();
  _deps.storage->clearAll();
  _identity = IoTLabsRuntimeIdentity();
  _connection.clearCredentials();
  _activation.clearToken();
  _activation.unblock();

  if (restartDevice) {
    iotlabs_log::line(_deps.logger, "[IoTLabs] Factory reset complete; restarting");
    _deps.clock->restartDevice();
  }

  return startProvisioningInternal();
}

bool IoTLabsEngine::publishTelemetrySnapshot() { return _runtime.publishTelemetrySnapshot(); }
bool IoTLabsEngine::publishStatus() { return _runtime.publishStatus(); }

uint32_t IoTLabsEngine::reconnectDelayMs() const {
  if (!_connection.connected()) return _connection.backoffMs();
  if (!_identity.valid()) return _activation.backoffMs();
  return _runtime.backoffMs();
}
