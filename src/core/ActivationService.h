#pragma once

#include <cstdint>
#include <string>

#include "../IoTLabsTypes.h"
#include "IoTLabsRuntimeIdentity.h"
#include "Log.h"
#include "ports/IClock.h"
#include "ports/IHttpClient.h"

// HTTPS activation against the Device Platform bootstrap endpoint
// (IoTLabs internal design notes), ported 1:1 from
// the original serviceActivation()/attemptActivation() for identical retry
// and error-classification behavior.
class ActivationService {
 public:
  struct Config {
    const char* deviceTemplateId = nullptr;
    uint32_t deviceTemplateVersion = 1;
    const char* firmwareVersion = nullptr;
    const char* provisioningUrl = nullptr;
    const char* provisioningPath = "/provisioning";
    bool tlsInsecure = false;
    const char* provisioningRootCA = nullptr;
    uint32_t httpTimeoutMs = 8000;
    uint32_t retryMinMs = 2000;
    uint32_t retryMaxMs = 30000;
  };

  enum class Outcome { NOT_DUE, WAITING_FOR_TOKEN, SUCCESS, BLOCKED, RETRY_SCHEDULED };

  using EventCallback = void (*)(void* context, IoTLabsEvent event);

  ActivationService(IHttpClient& http, IClock& clock, ILogger* logger);

  void configure(const Config& config, const std::string& hardwareId);
  void setEventCallback(EventCallback callback, void* context);

  void setToken(const std::string& token);
  void clearToken();
  bool hasToken() const { return !_token.empty(); }

  void resetBackoff();
  void requestImmediateAttempt(uint32_t now);

  Outcome tick(uint32_t now, IoTLabsRuntimeIdentity& outIdentity, std::string& outErrorCode);
  bool blocked() const { return _blocked; }
  const std::string& lastError() const { return _lastError; }
  uint32_t backoffMs() const { return _backoffMs; }

  // Escape hatch for failures that happen after a successful HTTP exchange
  // but outside this service's control (e.g. the caller's storage write).
  void forceBlocked(const char* errorCode);
  void unblock() { _blocked = false; }

 private:
  IHttpClient& _http;
  IClock& _clock;
  ILogger* _logger;

  Config _config;
  std::string _hardwareId;
  std::string _token;

  bool _blocked = false;
  std::string _lastError;
  uint32_t _backoffMs = 2000;
  uint32_t _nextAttemptAt = 0;

  EventCallback _eventCallback = nullptr;
  void* _eventContext = nullptr;

  void emit(IoTLabsEvent event);
  std::string provisioningUrl() const;
  static bool errorIsBlocking(const std::string& code);
  bool attempt(IoTLabsRuntimeIdentity& outIdentity, std::string& outErrorCode);
};
