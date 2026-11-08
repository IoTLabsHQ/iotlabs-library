#include "ActivationService.h"

#include <ArduinoJson.h>

#include <cstring>

namespace {
bool due(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
}  // namespace

ActivationService::ActivationService(IHttpClient& http, IClock& clock, ILogger* logger)
    : _http(http), _clock(clock), _logger(logger) {}

void ActivationService::configure(const Config& config, const std::string& hardwareId) {
  _config = config;
  _hardwareId = hardwareId;
  resetBackoff();
}

void ActivationService::setEventCallback(EventCallback callback, void* context) {
  _eventCallback = callback;
  _eventContext = context;
}

void ActivationService::emit(IoTLabsEvent event) {
  if (_eventCallback) _eventCallback(_eventContext, event);
}

void ActivationService::setToken(const std::string& token) {
  _token = token;
  _blocked = false;
  resetBackoff();
}

void ActivationService::clearToken() {
  _token.clear();
}

void ActivationService::forceBlocked(const char* errorCode) {
  // No self-emit: the caller (IoTLabsEngine) owns lastError/ERROR sequencing
  // so a single setError() call stays the one place that orders "record the
  // code" before "notify the app", matching the original monolith.
  _lastError = errorCode ? errorCode : "";
  _blocked = true;
}

void ActivationService::resetBackoff() {
  _backoffMs = _config.retryMinMs > 1000 ? _config.retryMinMs : 1000;
}

void ActivationService::requestImmediateAttempt(uint32_t now) {
  _nextAttemptAt = now;
}

std::string ActivationService::provisioningUrl() const {
  std::string base(_config.provisioningUrl ? _config.provisioningUrl : "");
  while (!base.empty() && base.back() == '/') base.pop_back();
  std::string path(_config.provisioningPath ? _config.provisioningPath : "/provisioning");
  if (path.empty() || path.front() != '/') path = "/" + path;
  return base + path;
}

bool ActivationService::errorIsBlocking(const std::string& code) {
  return code == "INVALID_TOKEN" || code == "TOKEN_EXPIRED" || code == "TOKEN_ALREADY_USED" ||
         code == "DEVICE_NOT_FOUND" || code == "DEVICE_REVOKED" || code == "DEVICE_TEMPLATE_MISMATCH" ||
         code == "HARDWARE_ALREADY_CLAIMED" || code == "QUOTA_EXCEEDED" ||
         code == "UNSUPPORTED_PROTOCOL" || code == "UNSUPPORTED_PROVISIONING_VERSION" ||
         code == "UNSUPPORTED_PROVISIONING_ACTION" || code == "UNSUPPORTED_FIRMWARE";
}

bool ActivationService::attempt(IoTLabsRuntimeIdentity& outIdentity, std::string& outErrorCode) {
  if (!_config.tlsInsecure && (!_config.provisioningRootCA || strlen(_config.provisioningRootCA) == 0)) {
    outErrorCode = "PROVISIONING_ROOT_CA_REQUIRED";
    return false;
  }

  // Generic provisioning envelope -- {version, action, data}. `version` is
  // the wire SHAPE of `data` for this action (dispatch key only, distinct
  // from any business "provisioningVersion" concept); server auto-detects
  // compatibility from it, so future protocol changes are added as new
  // (version, action) cases server-side without a new URL/SDK release.
  JsonDocument envelope;
  envelope["version"] = IOTLABS_PROVISIONING_VERSION;
  envelope["action"] = "activate";
  JsonObject data = envelope["data"].to<JsonObject>();
  data["activationToken"] = _token;
  data["hardwareId"] = _hardwareId;
  data["deviceTemplateId"] = _config.deviceTemplateId;
  data["deviceTemplateVersion"] = _config.deviceTemplateVersion;
  data["platform"] = "esp32";
  data["firmwareVersion"] = _config.firmwareVersion;
  data["sdkVersion"] = IOTLABS_SDK_VERSION;
  data["protocolVersion"] = IOTLABS_PROTOCOL_VERSION;

  std::string requestBody;
  serializeJson(envelope, requestBody);

  const std::string url = provisioningUrl();
  iotlabs_log::line(_logger, "[IoTLabs] Activating with Device Platform: %s", url.c_str());

  IoTLabsHttpResponse response;
  const bool sent = _http.postJson(url.c_str(), requestBody.c_str(), _config.httpTimeoutMs,
                                    _config.tlsInsecure, _config.provisioningRootCA, response);
  if (!sent) {
    outErrorCode = "ACTIVATION_HTTP_BEGIN_FAILED";
    return false;
  }

  if (response.statusCode < 200 || response.statusCode >= 300) {
    std::string code = "ACTIVATION_FAILED";
    if (!response.body.empty()) {
      JsonDocument err;
      if (!deserializeJson(err, response.body)) {
        const char* parsed = err["error"]["code"] | "";
        if (strlen(parsed) > 0) code = parsed;
      }
    }
    outErrorCode = code;
    iotlabs_log::line(_logger, "[IoTLabs] Activation failed HTTP=%d code=%s", response.statusCode, code.c_str());
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response.body)) {
    outErrorCode = "ACTIVATION_RESPONSE_INVALID";
    return false;
  }

  outIdentity.deviceId = std::string(doc["device"]["id"] | "");
  outIdentity.deviceTemplateId = std::string(
      doc["device"]["deviceTemplateId"] | (_config.deviceTemplateId ? _config.deviceTemplateId : ""));
  outIdentity.deviceTemplateVersion = doc["device"]["deviceTemplateVersion"] | _config.deviceTemplateVersion;
  outIdentity.mqttClientId = std::string(doc["mqtt"]["clientId"] | "");
  outIdentity.mqttUsername = std::string(doc["mqtt"]["username"] | "");
  outIdentity.mqttPassword = std::string(doc["mqtt"]["password"] | "");
  outIdentity.mqttBroker = std::string(doc["mqtt"]["broker"] | "");
  outIdentity.mqttPort = doc["mqtt"]["port"] | 8883;
  outIdentity.topicPrefix = std::string(doc["mqtt"]["topicPrefix"] | "");

  if (!outIdentity.valid()) {
    outErrorCode = "ACTIVATION_CREDENTIALS_INVALID";
    return false;
  }
  if (_config.deviceTemplateId && outIdentity.deviceTemplateId != _config.deviceTemplateId) {
    outErrorCode = "ACTIVATION_DEVICE_TEMPLATE_MISMATCH";
    return false;
  }

  return true;
}

ActivationService::Outcome ActivationService::tick(uint32_t now, IoTLabsRuntimeIdentity& outIdentity,
                                                     std::string& outErrorCode) {
  if (_blocked) return Outcome::NOT_DUE;
  if (_token.empty()) return Outcome::WAITING_FOR_TOKEN;
  if (!due(now, _nextAttemptAt)) return Outcome::NOT_DUE;

  emit(IoTLabsEvent::ACTIVATING);

  std::string errorCode;
  if (attempt(outIdentity, errorCode)) {
    resetBackoff();
    _lastError.clear();
    _blocked = false;
    // Caller emits ACTIVATED only after it has durably saved outIdentity,
    // matching the original attemptActivation() ordering (save before notify).
    return Outcome::SUCCESS;
  }

  _lastError = errorCode;
  outErrorCode = errorCode;

  if (errorIsBlocking(errorCode) || errorCode == "ACTIVATION_DEVICE_TEMPLATE_MISMATCH" ||
      errorCode == "PROVISIONING_ROOT_CA_REQUIRED") {
    _blocked = true;
    // Caller (setError) owns lastError + ERROR emission ordering.
    return Outcome::BLOCKED;
  }

  _nextAttemptAt = now + _backoffMs;
  _backoffMs = (_backoffMs * 2) < _config.retryMaxMs ? (_backoffMs * 2) : _config.retryMaxMs;
  return Outcome::RETRY_SCHEDULED;
}
