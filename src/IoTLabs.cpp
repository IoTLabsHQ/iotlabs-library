#include "IoTLabs.h"

IoTLabs* IoTLabs::_instance = nullptr;

// ============================================================
// IoTLabsValue
// ============================================================

bool IoTLabsValue::asBool(bool fallback) const {
  if (_value.is<bool>()) return _value.as<bool>();
  if (_value.is<int>() || _value.is<long>()) return _value.as<long>() != 0;
  if (_value.is<const char*>()) {
    String s = _value.as<const char*>();
    s.toLowerCase();
    if (s == "on" || s == "true" || s == "1") return true;
    if (s == "off" || s == "false" || s == "0") return false;
  }
  return fallback;
}

long IoTLabsValue::asInt(long fallback) const {
  if (_value.is<long>() || _value.is<int>()) return _value.as<long>();
  if (_value.is<float>() || _value.is<double>()) return static_cast<long>(_value.as<double>());
  return fallback;
}

float IoTLabsValue::asFloat(float fallback) const {
  if (_value.is<float>() || _value.is<double>() || _value.is<long>() || _value.is<int>()) {
    return _value.as<float>();
  }
  return fallback;
}

String IoTLabsValue::asString(const String& fallback) const {
  if (_value.is<const char*>()) return String(_value.as<const char*>());
  return fallback;
}

bool IoTLabsValue::has(const char* key) const {
  if (!_value.is<JsonObjectConst>()) return false;
  return _value.as<JsonObjectConst>().containsKey(key);
}

IoTLabsValue IoTLabsValue::operator[](const char* key) const {
  if (!_value.is<JsonObjectConst>()) return IoTLabsValue(JsonVariantConst());
  return IoTLabsValue(_value.as<JsonObjectConst>()[key]);
}

// ============================================================
// IoTLabs lifecycle
// ============================================================

IoTLabs::IoTLabs() : _mqtt(_mqttSecureClient), _provisionServer(80) {}

bool IoTLabs::due(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool IoTLabs::validateConfig() {
  if (!_config.profileId || strlen(_config.profileId) == 0) {
    setError("PROFILE_ID_REQUIRED");
    return false;
  }
  if (!_config.firmwareVersion || strlen(_config.firmwareVersion) == 0) {
    setError("FIRMWARE_VERSION_REQUIRED");
    return false;
  }
  if (!_config.devicePlatformUrl || strlen(_config.devicePlatformUrl) == 0) {
    setError("DEVICE_PLATFORM_URL_REQUIRED");
    return false;
  }
  return true;
}

bool IoTLabs::begin(const IoTLabsConfig& config) {
  if (_started) return true;

  _config = config;
  setStatus(IoTLabsStatus::BOOT);
  emit(IoTLabsEvent::BOOT);

  if (!validateConfig()) return false;
  if (!_storage.begin(_config.storageNamespace)) {
    setError("STORAGE_INIT_FAILED");
    return false;
  }

  generateHardwareId();
  loadPersistentState();

  if (_runtime.valid() && _runtime.profileId.length() > 0 && _runtime.profileId != _config.profileId) {
    setError("STORED_PROFILE_MISMATCH");
    return false;
  }

  _wifiBackoffMs = (_config.reconnectMinMs > 1000 ? _config.reconnectMinMs : 1000);
  _mqttBackoffMs = (_config.reconnectMinMs > 1000 ? _config.reconnectMinMs : 1000);
  _activationBackoffMs = (_config.activationRetryMinMs > 1000 ? _config.activationRetryMinMs : 1000);

  _mqtt.setCallback(IoTLabs::mqttCallbackThunk);
  _mqtt.setKeepAlive(_config.keepAliveSeconds);
  _mqtt.setSocketTimeout(_config.socketTimeoutSeconds);
  _mqtt.setBufferSize(_config.mqttBufferBytes);

  _instance = this;
  _started = true;

  Serial.println();
  Serial.print("[IoTLabs] SDK ");
  Serial.println(IOTLABS_SDK_VERSION);
  Serial.print("[IoTLabs] Hardware ID: ");
  Serial.println(_hardwareId);
  Serial.print("[IoTLabs] Profile: ");
  Serial.print(_config.profileId);
  Serial.print(" v");
  Serial.println(_config.profileVersion);

  const uint32_t now = millis();
  _nextWifiAttemptAt = now;
  _nextMqttAttemptAt = now;
  _nextActivationAttemptAt = now;

  if (_runtime.valid()) {
    if (!configureMqtt()) return false;
    if (_wifiSsid.length() == 0) {
      Serial.println("[IoTLabs] Device identity exists but WiFi is not configured");
      startProvisioningPortal();
    } else {
      WiFi.mode(WIFI_STA);
      startWifiAttempt(now);
    }
    return true;
  }

  // Unprovisioned devices keep SoftAP available while they join WiFi and activate.
  startProvisioningPortal();
  if (_wifiSsid.length() > 0) startWifiAttempt(now);
  return true;
}

void IoTLabs::loop() {
  if (!_started) return;

  if (_provisioningActive) _provisionServer.handleClient();

  const uint32_t now = millis();
  serviceWiFi(now);

  if (WiFi.status() != WL_CONNECTED) return;

  if (!_runtime.valid()) {
    serviceActivation(now);
    return;
  }

  serviceMqtt(now);
  if (!_mqtt.connected()) return;

  _mqtt.loop();

  if (_config.telemetryIntervalMs > 0 &&
      due(now, _lastTelemetryAt + _config.telemetryIntervalMs)) {
    _lastTelemetryAt = now;
    publishTelemetrySnapshot();
  }

  if (_config.statusIntervalMs > 0 &&
      due(now, _lastStatusAt + _config.statusIntervalMs)) {
    _lastStatusAt = now;
    publishStatus();
  }
}

void IoTLabs::generateHardwareId() {
  const uint64_t chip = ESP.getEfuseMac();
  char buffer[13];
  snprintf(buffer, sizeof(buffer), "%04X%08X",
           static_cast<uint16_t>(chip >> 32),
           static_cast<uint32_t>(chip));
  _hardwareId = String(buffer);
}

void IoTLabs::loadPersistentState() {
  _wifiSsid = _storage.wifiSsid();
  _wifiPassword = _storage.wifiPassword();
  _activationToken = _storage.activationToken();
  _runtime = _storage.runtimeIdentity();
}

bool IoTLabs::connected() const { return _mqtt.connected(); }
bool IoTLabs::wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
bool IoTLabs::provisioned() const { return _runtime.valid(); }
bool IoTLabs::provisioning() const { return _provisioningActive && !_runtime.valid(); }
IoTLabsStatus IoTLabs::status() const { return _status; }
void IoTLabs::onEvent(EventHandler handler) { _eventHandler = handler; }
int IoTLabs::mqttState() const { return _mqtt.state(); }
uint32_t IoTLabs::reconnectDelayMs() const {
  if (WiFi.status() != WL_CONNECTED) return _wifiBackoffMs;
  if (!_runtime.valid()) return _activationBackoffMs;
  return _mqttBackoffMs;
}

void IoTLabs::emit(IoTLabsEvent event) {
  if (_eventHandler) _eventHandler(event);
}

void IoTLabs::setStatus(IoTLabsStatus status) { _status = status; }

void IoTLabs::setError(const String& code) {
  _lastError = code;
  setStatus(IoTLabsStatus::ERROR);
  Serial.print("[IoTLabs] ERROR: ");
  Serial.println(code);
  emit(IoTLabsEvent::ERROR);
}

// ============================================================
// Provisioning portal
// ============================================================

bool IoTLabs::startProvisioning() {
  if (!_started) return false;
  return startProvisioningPortal();
}

bool IoTLabs::startProvisioningPortal() {
  if (_provisioningActive) return true;

  WiFi.mode(WIFI_AP_STA);
  const String suffix = _hardwareId.length() >= 6
      ? _hardwareId.substring(_hardwareId.length() - 6)
      : _hardwareId;
  _apName = String(_config.provisioningApPrefix ? _config.provisioningApPrefix : "IoTLabs") + "-" + suffix;

  bool ok = false;
  if (_config.provisioningApPassword && strlen(_config.provisioningApPassword) >= 8) {
    ok = WiFi.softAP(_apName.c_str(), _config.provisioningApPassword);
  } else {
    ok = WiFi.softAP(_apName.c_str());
    Serial.println("[IoTLabs] WARNING: provisioning SoftAP is open; development use only");
  }

  if (!ok) {
    setError("PROVISIONING_AP_FAILED");
    return false;
  }

  configureProvisioningRoutes();
  _provisionServer.begin();
  _provisioningActive = true;
  _activationBlocked = false;

  setStatus(_runtime.valid() ? IoTLabsStatus::PROVISIONING : IoTLabsStatus::UNPROVISIONED);
  emit(IoTLabsEvent::PROVISIONING_STARTED);

  Serial.print("[IoTLabs] Provisioning AP: ");
  Serial.println(_apName);
  Serial.print("[IoTLabs] Local API: http://");
  Serial.println(WiFi.softAPIP());
  return true;
}

void IoTLabs::stopProvisioningPortal() {
  if (!_provisioningActive) return;
  _provisionServer.stop();
  WiFi.softAPdisconnect(true);
  _provisioningActive = false;
  if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
}

void IoTLabs::configureProvisioningRoutes() {
  if (_provisionRoutesConfigured) return;
  _provisionRoutesConfigured = true;
  _provisionServer.on("/device-info", HTTP_GET, [this]() { handleDeviceInfoRequest(); });
  _provisionServer.on("/wifi/networks", HTTP_GET, [this]() { handleWifiNetworksRequest(); });
  _provisionServer.on("/provision", HTTP_POST, [this]() { handleProvisionRequest(); });
  _provisionServer.onNotFound([this]() {
    sendLocalError(404, "NOT_FOUND", "Unknown provisioning endpoint");
  });
}

void IoTLabs::handleDeviceInfoRequest() {
  JsonDocument doc;
  doc["platform"] = "esp32";
  doc["hardwareId"] = _hardwareId;
  doc["profileId"] = _config.profileId;
  doc["profileVersion"] = _config.profileVersion;
  doc["firmwareVersion"] = _config.firmwareVersion;
  doc["sdkVersion"] = IOTLABS_SDK_VERSION;
  doc["protocolVersion"] = IOTLABS_PROTOCOL_VERSION;
  doc["provisioningVersion"] = IOTLABS_PROVISIONING_VERSION;
  doc["provisioned"] = _runtime.valid();
  if (_runtime.valid()) doc["deviceId"] = _runtime.deviceId;

  String body;
  serializeJson(doc, body);
  _provisionServer.send(200, "application/json", body);
}

void IoTLabs::handleWifiNetworksRequest() {
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  const int count = WiFi.scanNetworks(false, true);
  for (int i = 0; i < count; ++i) {
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();

  String body;
  serializeJson(doc, body);
  _provisionServer.send(200, "application/json", body);
}

void IoTLabs::handleProvisionRequest() {
  const String raw = _provisionServer.arg("plain");
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, raw);
  if (error) {
    sendLocalError(400, "INVALID_REQUEST", "Invalid JSON payload");
    return;
  }

  const char* ssid = doc["wifi"]["ssid"] | "";
  const char* password = doc["wifi"]["password"] | "";
  const char* token = doc["activation"]["token"] | "";

  if (strlen(ssid) == 0) {
    sendLocalError(422, "WIFI_SSID_REQUIRED", "wifi.ssid is required");
    return;
  }

  if (!_runtime.valid() && strlen(token) == 0) {
    sendLocalError(422, "ACTIVATION_TOKEN_REQUIRED", "activation.token is required for an unprovisioned device");
    return;
  }

  _wifiSsid = ssid;
  _wifiPassword = password;
  if (!_storage.saveWifi(_wifiSsid, _wifiPassword)) {
    sendLocalError(500, "STORAGE_WRITE_FAILED", "Could not store WiFi configuration");
    return;
  }

  if (strlen(token) > 0) {
    _activationToken = token;
    _storage.saveActivationToken(_activationToken);
    _activationBlocked = false;
    _activationBackoffMs = (_config.activationRetryMinMs > 1000 ? _config.activationRetryMinMs : 1000);
    _nextActivationAttemptAt = millis();
  }

  JsonDocument response;
  response["accepted"] = true;
  response["mode"] = _runtime.valid() ? "wifi_reconfigure" : "provision";
  String body;
  serializeJson(response, body);
  _provisionServer.send(202, "application/json", body);

  emit(IoTLabsEvent::PROVISIONING_ACCEPTED);
  _nextWifiAttemptAt = millis() + 100;
  _wifiAttemptActive = false;
}

void IoTLabs::sendLocalError(int httpStatus, const char* code, const char* message) {
  JsonDocument doc;
  doc["error"]["code"] = code;
  doc["error"]["message"] = message;
  String body;
  serializeJson(doc, body);
  _provisionServer.send(httpStatus, "application/json", body);
}

bool IoTLabs::resetWiFi() {
  if (!_started) return false;
  _storage.clearWifi();
  _wifiSsid = String();
  _wifiPassword = String();
  _wifiAttemptActive = false;
  WiFi.disconnect(false, true);
  return startProvisioningPortal();
}

bool IoTLabs::factoryReset(bool restartDevice) {
  if (!_started) return false;
  _mqtt.disconnect();
  _storage.clearAll();
  _runtime = IoTLabsRuntimeIdentity();
  _wifiSsid = String();
  _wifiPassword = String();
  _activationToken = String();
  _mqttConfigured = false;
  _activationBlocked = false;

  if (restartDevice) {
    Serial.println("[IoTLabs] Factory reset complete; restarting");
    delay(100);
    ESP.restart();
  }

  return startProvisioningPortal();
}

// ============================================================
// WiFi
// ============================================================

void IoTLabs::startWifiAttempt(uint32_t now) {
  if (_wifiSsid.length() == 0) return;

  WiFi.mode(_provisioningActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.print("[IoTLabs] WiFi connecting to ");
  Serial.println(_wifiSsid);
  emit(IoTLabsEvent::WIFI_CONNECTING);
  setStatus(IoTLabsStatus::WIFI_CONNECTING);

  WiFi.begin(_wifiSsid.c_str(), _wifiPassword.c_str());
  _wifiAttemptStartedAt = now;
  _wifiAttemptActive = true;
  _nextWifiAttemptAt = now + _config.wifiConnectTimeoutMs;
}

void IoTLabs::serviceWiFi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    _wifiAttemptActive = false;
    if (!_seenWifiConnected) {
      _seenWifiConnected = true;
      _wifiBackoffMs = (_config.reconnectMinMs > 1000 ? _config.reconnectMinMs : 1000);
      _nextMqttAttemptAt = now;
      _nextActivationAttemptAt = now;
      Serial.print("[IoTLabs] WiFi connected, IP=");
      Serial.println(WiFi.localIP());
      emit(IoTLabsEvent::WIFI_CONNECTED);
      if (_runtime.valid() && _provisioningActive) stopProvisioningPortal();
    }
    return;
  }

  if (_seenWifiConnected) {
    _seenWifiConnected = false;
    _seenMqttConnected = false;
    if (_mqtt.connected()) _mqtt.disconnect();
    setStatus(IoTLabsStatus::OFFLINE);
    emit(IoTLabsEvent::OFFLINE);
    Serial.println("[IoTLabs] WiFi disconnected");
  }

  if (_wifiSsid.length() == 0) {
    if (!_provisioningActive) startProvisioningPortal();
    return;
  }

  if (_wifiAttemptActive) {
    if (!due(now, _wifiAttemptStartedAt + _config.wifiConnectTimeoutMs)) return;

    _wifiAttemptActive = false;
    WiFi.disconnect(false, false);
    Serial.println("[IoTLabs] WiFi connection attempt timed out");
    emit(IoTLabsEvent::WIFI_FAILED);

    if (!_runtime.valid() && !_provisioningActive) startProvisioningPortal();
    _nextWifiAttemptAt = now + _wifiBackoffMs;
    _wifiBackoffMs = ((_wifiBackoffMs * 2) < _config.reconnectMaxMs ? (_wifiBackoffMs * 2) : _config.reconnectMaxMs);
    return;
  }

  if (due(now, _nextWifiAttemptAt)) startWifiAttempt(now);
}

// ============================================================
// Activation
// ============================================================

String IoTLabs::activationUrl() const {
  String base(_config.devicePlatformUrl ? _config.devicePlatformUrl : "");
  while (base.endsWith("/")) base.remove(base.length() - 1);
  String path(_config.activationPath ? _config.activationPath : "/api/device/v1/activate");
  if (!path.startsWith("/")) path = "/" + path;
  return base + path;
}

bool IoTLabs::activationErrorIsBlocking(const String& code) const {
  return code == "INVALID_TOKEN" ||
         code == "TOKEN_EXPIRED" ||
         code == "TOKEN_ALREADY_USED" ||
         code == "DEVICE_NOT_FOUND" ||
         code == "DEVICE_REVOKED" ||
         code == "PROFILE_MISMATCH" ||
         code == "HARDWARE_ALREADY_CLAIMED" ||
         code == "QUOTA_EXCEEDED" ||
         code == "UNSUPPORTED_PROTOCOL" ||
         code == "UNSUPPORTED_PROVISIONING_VERSION" ||
         code == "UNSUPPORTED_FIRMWARE";
}

void IoTLabs::serviceActivation(uint32_t now) {
  if (_runtime.valid() || _activationBlocked) return;
  if (_activationToken.length() == 0) {
    setStatus(IoTLabsStatus::PROVISIONING);
    if (!_provisioningActive) startProvisioningPortal();
    return;
  }
  if (!due(now, _nextActivationAttemptAt)) return;

  setStatus(IoTLabsStatus::ACTIVATING);
  emit(IoTLabsEvent::ACTIVATING);

  if (attemptActivation()) {
    _activationBackoffMs = (_config.activationRetryMinMs > 1000 ? _config.activationRetryMinMs : 1000);
    _nextMqttAttemptAt = millis();
    stopProvisioningPortal();
    return;
  }

  if (_activationBlocked) {
    if (!_provisioningActive) startProvisioningPortal();
    return;
  }

  _nextActivationAttemptAt = now + _activationBackoffMs;
  _activationBackoffMs = ((_activationBackoffMs * 2) < _config.activationRetryMaxMs ? (_activationBackoffMs * 2) : _config.activationRetryMaxMs);
}

bool IoTLabs::attemptActivation() {
  WiFiClientSecure client;
  if (_config.tlsInsecure) {
    client.setInsecure();
    Serial.println("[IoTLabs] WARNING: activation TLS verification disabled");
  } else {
    if (!_config.devicePlatformRootCA || strlen(_config.devicePlatformRootCA) == 0) {
      setError("DEVICE_PLATFORM_ROOT_CA_REQUIRED");
      _activationBlocked = true;
      return false;
    }
    client.setCACert(_config.devicePlatformRootCA);
  }

  HTTPClient http;
  http.setTimeout(_config.activationHttpTimeoutMs);
  const String url = activationUrl();
  if (!http.begin(client, url)) {
    _lastError = "ACTIVATION_HTTP_BEGIN_FAILED";
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  JsonDocument request;
  request["activationToken"] = _activationToken;
  request["hardwareId"] = _hardwareId;
  request["profileId"] = _config.profileId;
  request["profileVersion"] = _config.profileVersion;
  request["platform"] = "esp32";
  request["firmwareVersion"] = _config.firmwareVersion;
  request["sdkVersion"] = IOTLABS_SDK_VERSION;
  request["protocolVersion"] = IOTLABS_PROTOCOL_VERSION;
  request["provisioningVersion"] = IOTLABS_PROVISIONING_VERSION;

  String requestBody;
  serializeJson(request, requestBody);

  Serial.print("[IoTLabs] Activating with Device Platform: ");
  Serial.println(url);
  const int statusCode = http.POST(requestBody);
  const String responseBody = statusCode > 0 ? http.getString() : String();
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    String code = "ACTIVATION_FAILED";
    if (responseBody.length() > 0) {
      JsonDocument err;
      if (!deserializeJson(err, responseBody)) {
        const char* parsed = err["error"]["code"] | "";
        if (strlen(parsed) > 0) code = parsed;
      }
    }

    _lastError = code;
    Serial.print("[IoTLabs] Activation failed HTTP=");
    Serial.print(statusCode);
    Serial.print(" code=");
    Serial.println(code);

    if (activationErrorIsBlocking(code)) {
      _activationBlocked = true;
      setStatus(IoTLabsStatus::ERROR);
      emit(IoTLabsEvent::ERROR);
    }
    return false;
  }

  JsonDocument response;
  if (deserializeJson(response, responseBody)) {
    _lastError = "ACTIVATION_RESPONSE_INVALID";
    return false;
  }

  IoTLabsRuntimeIdentity identity;
  identity.deviceId = String(response["device"]["id"] | "");
  identity.profileId = String(response["device"]["profileId"] | _config.profileId);
  identity.profileVersion = response["device"]["profileVersion"] | _config.profileVersion;
  identity.mqttClientId = String(response["mqtt"]["clientId"] | "");
  identity.mqttUsername = String(response["mqtt"]["username"] | "");
  identity.mqttPassword = String(response["mqtt"]["password"] | "");
  identity.mqttBroker = String(response["mqtt"]["broker"] | "");
  identity.mqttPort = response["mqtt"]["port"] | 8883;
  identity.topicPrefix = String(response["mqtt"]["topicPrefix"] | "");

  if (!identity.valid()) {
    _lastError = "ACTIVATION_CREDENTIALS_INVALID";
    return false;
  }
  if (identity.profileId != _config.profileId) {
    setError("ACTIVATION_PROFILE_MISMATCH");
    _activationBlocked = true;
    return false;
  }

  if (!_storage.saveRuntimeIdentity(identity)) {
    setError("RUNTIME_IDENTITY_STORAGE_FAILED");
    _activationBlocked = true;
    return false;
  }

  _runtime = identity;
  _storage.clearActivationToken();
  _activationToken = String();
  _activationBlocked = false;
  _lastError = String();

  if (!configureMqtt()) return false;

  Serial.print("[IoTLabs] Activated Device ID: ");
  Serial.println(_runtime.deviceId);
  emit(IoTLabsEvent::ACTIVATED);
  return true;
}

// ============================================================
// MQTT runtime
// ============================================================

bool IoTLabs::configureMqtt() {
  if (!_runtime.valid()) return false;

  if (_config.tlsInsecure) {
    _mqttSecureClient.setInsecure();
    Serial.println("[IoTLabs] WARNING: MQTT TLS verification disabled");
  } else {
    const char* ca = (_config.mqttRootCA && strlen(_config.mqttRootCA) > 0)
        ? _config.mqttRootCA
        : _config.devicePlatformRootCA;
    if (!ca || strlen(ca) == 0) {
      setError("MQTT_ROOT_CA_REQUIRED");
      return false;
    }
    _mqttSecureClient.setCACert(ca);
  }

  _mqtt.setServer(_runtime.mqttBroker.c_str(), _runtime.mqttPort);
  buildTopics();
  _mqttConfigured = true;
  return true;
}

void IoTLabs::buildTopics() {
  String base = _runtime.topicPrefix;
  if (base.length() == 0) base = String("devices/") + _runtime.mqttClientId;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  _topicTelemetry = base + "/telemetry";
  _topicCmd = base + "/cmd";
  _topicStatus = base + "/status";
  _topicPing = base + "/ping";
}

void IoTLabs::serviceMqtt(uint32_t now) {
  if (!_mqttConfigured && !configureMqtt()) return;

  if (_mqtt.connected()) {
    if (!_seenMqttConnected) {
      _seenMqttConnected = true;
      onMqttConnected();
    }
    return;
  }

  if (_seenMqttConnected) {
    _seenMqttConnected = false;
    setStatus(IoTLabsStatus::OFFLINE);
    emit(IoTLabsEvent::OFFLINE);
    Serial.println("[IoTLabs] Cloud disconnected");
  }

  if (!due(now, _nextMqttAttemptAt)) return;

  setStatus(IoTLabsStatus::CLOUD_CONNECTING);
  emit(IoTLabsEvent::CLOUD_CONNECTING);

  if (connectMqtt()) {
    _mqttBackoffMs = (_config.reconnectMinMs > 1000 ? _config.reconnectMinMs : 1000);
    _nextMqttAttemptAt = now;
  } else {
    _nextMqttAttemptAt = now + _mqttBackoffMs;
    _mqttBackoffMs = ((_mqttBackoffMs * 2) < _config.reconnectMaxMs ? (_mqttBackoffMs * 2) : _config.reconnectMaxMs);
  }
}

bool IoTLabs::connectMqtt() {
  Serial.print("[IoTLabs] Cloud connecting...");

  JsonDocument will;
  will["status"] = "offline";
  will["device_id"] = _runtime.deviceId;
  will["profile_id"] = _config.profileId;
  will["sdk_version"] = IOTLABS_SDK_VERSION;
  String willPayload;
  serializeJson(will, willPayload);

  const bool ok = _mqtt.connect(
      _runtime.mqttClientId.c_str(),
      _runtime.mqttUsername.c_str(),
      _runtime.mqttPassword.c_str(),
      _topicStatus.c_str(),
      0,
      true,
      willPayload.c_str());

  if (!ok) {
    Serial.print(" failed rc=");
    Serial.println(_mqtt.state());
    return false;
  }

  Serial.println(" OK");
  _seenMqttConnected = true;
  onMqttConnected();
  return true;
}

void IoTLabs::onMqttConnected() {
  _mqtt.subscribe(_topicCmd.c_str());
  _mqtt.subscribe(_topicPing.c_str());

  setStatus(IoTLabsStatus::ONLINE);
  publishStatus();
  publishTelemetrySnapshot();

  _lastTelemetryAt = millis();
  _lastStatusAt = millis();

  Serial.println("[IoTLabs] ONLINE");
  emit(IoTLabsEvent::ONLINE);
}

void IoTLabs::mqttCallbackThunk(char* topic, byte* payload, unsigned int length) {
  if (_instance) _instance->handleMessage(topic, payload, length);
}

void IoTLabs::handleMessage(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; ++i) message += static_cast<char>(payload[i]);

  Serial.print("[IoTLabs] RX [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);

  const String topicString(topic);
  if (topicString == _topicPing) {
    handlePing(message);
    return;
  }
  if (topicString == _topicCmd) handleCommand(message);
}

void IoTLabs::handlePing(const String& message) {
  JsonDocument incoming;
  if (!deserializeJson(incoming, message)) {
    const char* type = incoming["type"] | "";
    if (String(type) == "pong") return; // Prevent self-echo loops on brokers that echo own publish.
  }

  JsonDocument pong;
  pong["type"] = "pong";
  pong["device_id"] = _runtime.deviceId;
  pong["hardware_id"] = _hardwareId;
  pong["uptime_ms"] = millis();
  publishJson(_topicPing, pong, false);
}

void IoTLabs::handleCommand(const String& message) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print("[IoTLabs] Invalid command JSON: ");
    Serial.println(error.c_str());
    emit(IoTLabsEvent::MESSAGE_ERROR);
    return;
  }

  const char* target = doc["target"] | "";
  const char* command = doc["command"] | "";
  if (strlen(target) == 0 || strlen(command) == 0) {
    Serial.println("[IoTLabs] Command requires target and command");
    emit(IoTLabsEvent::MESSAGE_ERROR);
    return;
  }

  HandlerEntry* handler = findHandler(String(target));
  if (!handler) {
    Serial.print("[IoTLabs] Unknown control: ");
    Serial.println(target);
    emit(IoTLabsEvent::MESSAGE_ERROR);
    return;
  }

  JsonVariantConst value = doc["value"];
  switch (handler->kind) {
    case HandlerKind::BOOL: {
      bool parsed = false;
      if (!parseBool(value, parsed)) {
        Serial.println("[IoTLabs] Expected boolean/on/off value");
        emit(IoTLabsEvent::MESSAGE_ERROR);
        return;
      }
      if (handler->boolHandler) handler->boolHandler(parsed);
      break;
    }
    case HandlerKind::NUMBER:
      if (!(value.is<int>() || value.is<long>() || value.is<float>() || value.is<double>())) {
        Serial.println("[IoTLabs] Expected numeric value");
        emit(IoTLabsEvent::MESSAGE_ERROR);
        return;
      }
      if (handler->numberHandler) handler->numberHandler(value.as<float>());
      break;
    case HandlerKind::STRING:
      if (!value.is<const char*>()) {
        Serial.println("[IoTLabs] Expected string value");
        emit(IoTLabsEvent::MESSAGE_ERROR);
        return;
      }
      if (handler->stringHandler) handler->stringHandler(String(value.as<const char*>()));
      break;
    case HandlerKind::JSON:
      if (handler->jsonHandler) handler->jsonHandler(IoTLabsValue(value));
      break;
    default:
      emit(IoTLabsEvent::MESSAGE_ERROR);
      return;
  }

  // Status includes only actual states reported by the application.
  publishStatus();
}

bool IoTLabs::parseBool(JsonVariantConst value, bool& result) {
  if (value.is<bool>()) {
    result = value.as<bool>();
    return true;
  }
  if (value.is<int>() || value.is<long>()) {
    result = value.as<long>() != 0;
    return true;
  }
  if (value.is<const char*>()) {
    String s(value.as<const char*>());
    s.toLowerCase();
    if (s == "on" || s == "true" || s == "1") {
      result = true;
      return true;
    }
    if (s == "off" || s == "false" || s == "0") {
      result = false;
      return true;
    }
  }
  return false;
}

// ============================================================
// Control registry
// ============================================================

IoTLabs::HandlerEntry* IoTLabs::findHandler(const String& controlId) {
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_handlers[i].used && _handlers[i].id == controlId) return &_handlers[i];
  }
  return nullptr;
}

IoTLabs::HandlerEntry* IoTLabs::findOrCreateHandler(const String& controlId) {
  HandlerEntry* existing = findHandler(controlId);
  if (existing) return existing;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_handlers[i].used) continue;
    _handlers[i].used = true;
    _handlers[i].id = controlId;
    return &_handlers[i];
  }
  Serial.println("[IoTLabs] Control handler registry full");
  return nullptr;
}

bool IoTLabs::on(const char* controlId, BoolHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreateHandler(String(controlId));
  if (!entry) return false;
  entry->kind = HandlerKind::BOOL;
  entry->boolHandler = handler;
  return true;
}

bool IoTLabs::on(const char* controlId, NumberHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreateHandler(String(controlId));
  if (!entry) return false;
  entry->kind = HandlerKind::NUMBER;
  entry->numberHandler = handler;
  return true;
}

bool IoTLabs::on(const char* controlId, StringHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreateHandler(String(controlId));
  if (!entry) return false;
  entry->kind = HandlerKind::STRING;
  entry->stringHandler = handler;
  return true;
}

bool IoTLabs::on(const char* controlId, JsonHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreateHandler(String(controlId));
  if (!entry) return false;
  entry->kind = HandlerKind::JSON;
  entry->jsonHandler = handler;
  return true;
}

IoTLabs::StateEntry* IoTLabs::findState(const String& controlId) {
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_states[i].used && _states[i].id == controlId) return &_states[i];
  }
  return nullptr;
}

IoTLabs::StateEntry* IoTLabs::findOrCreateState(const String& controlId) {
  StateEntry* existing = findState(controlId);
  if (existing) return existing;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_states[i].used) continue;
    _states[i].used = true;
    _states[i].id = controlId;
    return &_states[i];
  }
  Serial.println("[IoTLabs] State registry full");
  return nullptr;
}

bool IoTLabs::report(const char* controlId, bool value, bool publishNow) {
  if (!controlId) return false;
  StateEntry* state = findOrCreateState(String(controlId));
  if (!state) return false;
  state->kind = StateKind::BOOL;
  state->boolValue = value;
  return reportState(*state, publishNow);
}

bool IoTLabs::report(const char* controlId, int value, bool publishNow) {
  return report(controlId, static_cast<long>(value), publishNow);
}

bool IoTLabs::report(const char* controlId, long value, bool publishNow) {
  if (!controlId) return false;
  StateEntry* state = findOrCreateState(String(controlId));
  if (!state) return false;
  state->kind = StateKind::INTEGER;
  state->intValue = value;
  return reportState(*state, publishNow);
}

bool IoTLabs::report(const char* controlId, float value, bool publishNow) {
  return report(controlId, static_cast<double>(value), publishNow);
}

bool IoTLabs::report(const char* controlId, double value, bool publishNow) {
  if (!controlId) return false;
  StateEntry* state = findOrCreateState(String(controlId));
  if (!state) return false;
  state->kind = StateKind::NUMBER;
  state->numberValue = value;
  return reportState(*state, publishNow);
}

bool IoTLabs::report(const char* controlId, const char* value, bool publishNow) {
  return report(controlId, String(value ? value : ""), publishNow);
}

bool IoTLabs::report(const char* controlId, const String& value, bool publishNow) {
  if (!controlId) return false;
  StateEntry* state = findOrCreateState(String(controlId));
  if (!state) return false;
  state->kind = StateKind::STRING;
  state->stringValue = value;
  return reportState(*state, publishNow);
}

bool IoTLabs::report(const char* controlId, JsonVariantConst value, bool publishNow) {
  if (!controlId) return false;
  StateEntry* state = findOrCreateState(String(controlId));
  if (!state) return false;
  state->kind = StateKind::JSON;
  state->jsonValue = String();
  serializeJson(value, state->jsonValue);
  return reportState(*state, publishNow);
}

bool IoTLabs::reportState(StateEntry& state, bool publishNow) {
  if (!publishNow || !_mqtt.connected()) return true;
  return publishSingleState(state);
}

void IoTLabs::appendState(JsonDocument& doc, const StateEntry& state) const {
  switch (state.kind) {
    case StateKind::BOOL:
      doc[state.id] = state.boolValue ? "on" : "off";
      break;
    case StateKind::INTEGER:
      doc[state.id] = state.intValue;
      break;
    case StateKind::NUMBER:
      doc[state.id] = state.numberValue;
      break;
    case StateKind::STRING:
      doc[state.id] = state.stringValue;
      break;
    case StateKind::JSON: {
      JsonDocument nested;
      if (!deserializeJson(nested, state.jsonValue)) doc[state.id] = nested.as<JsonVariantConst>();
      break;
    }
    default:
      break;
  }
}

bool IoTLabs::publishSingleState(const StateEntry& state) {
  if (!_mqtt.connected()) return false;
  JsonDocument doc;
  appendState(doc, state);
  doc["timestamp"] = millis(); // Protocol v1 compatibility; Cloud may assign canonical wall-clock time.
  doc["uptime_ms"] = millis();
  return publishJson(_topicTelemetry, doc, false);
}

bool IoTLabs::publishTelemetrySnapshot() {
  if (!_mqtt.connected()) return false;

  JsonDocument doc;
  bool hasState = false;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (!_states[i].used || _states[i].kind == StateKind::NONE) continue;
    appendState(doc, _states[i]);
    hasState = true;
  }
  if (!hasState) return true;

  doc["timestamp"] = millis();
  doc["uptime_ms"] = millis();
  return publishJson(_topicTelemetry, doc, false);
}

bool IoTLabs::publishStatus() {
  if (!_mqtt.connected()) return false;

  JsonDocument doc;
  doc["status"] = "online";
  doc["device_id"] = _runtime.deviceId;
  doc["hardware_id"] = _hardwareId;
  doc["profile_id"] = _config.profileId;
  doc["profile_version"] = _config.profileVersion;
  doc["firmware_version"] = _config.firmwareVersion;
  doc["sdk_version"] = IOTLABS_SDK_VERSION;
  doc["protocol_version"] = IOTLABS_PROTOCOL_VERSION;
  doc["provisioning_version"] = IOTLABS_PROVISIONING_VERSION;

  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (!_states[i].used || _states[i].kind == StateKind::NONE) continue;
    appendState(doc, _states[i]);
  }

  doc["timestamp"] = millis();
  doc["uptime_ms"] = millis();
  return publishJson(_topicStatus, doc, true);
}

bool IoTLabs::publishJson(const String& topic, JsonDocument& doc, bool retained) {
  if (!_mqtt.connected()) return false;

  String payload;
  serializeJson(doc, payload);
  const bool ok = _mqtt.publish(topic.c_str(), payload.c_str(), retained);

  Serial.print("[IoTLabs] TX [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(payload);

  if (ok) emit(IoTLabsEvent::PUBLISHED);
  return ok;
}
