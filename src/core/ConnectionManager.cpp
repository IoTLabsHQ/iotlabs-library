#include "ConnectionManager.h"

namespace {
bool due(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
}  // namespace

ConnectionManager::ConnectionManager(INetwork& network, IClock& clock, ILogger* logger)
    : _network(network), _clock(clock), _logger(logger) {}

void ConnectionManager::configure(const Config& config) {
  _config = config;
  _backoffMs = config.reconnectMinMs > 1000 ? config.reconnectMinMs : 1000;
}

void ConnectionManager::setCredentials(const std::string& ssid, const std::string& password) {
  _ssid = ssid;
  _password = password;
}

void ConnectionManager::applyNewCredentials(const std::string& ssid, const std::string& password,
                                             uint32_t nextAttemptAt) {
  _ssid = ssid;
  _password = password;
  _attemptActive = false;
  _nextAttemptAt = nextAttemptAt;
}

void ConnectionManager::clearCredentials() {
  _ssid.clear();
  _password.clear();
  _attemptActive = false;
}

void ConnectionManager::resetBackoff() {
  _backoffMs = _config.reconnectMinMs > 1000 ? _config.reconnectMinMs : 1000;
}

void ConnectionManager::requestImmediateAttempt(uint32_t now) {
  _nextAttemptAt = now;
}

void ConnectionManager::startAttempt(uint32_t now, TickResult& result) {
  if (_ssid.empty()) return;

  _network.beginStation();
  iotlabs_log::line(_logger, "[IoTLabs] WiFi connecting to %s", _ssid.c_str());

  _network.connect(_ssid.c_str(), _password.c_str());
  _attemptStartedAt = now;
  _attemptActive = true;
  _nextAttemptAt = now + _config.wifiConnectTimeoutMs;
  result.attemptStarted = true;
}

ConnectionManager::TickResult ConnectionManager::tick(uint32_t now) {
  TickResult result;

  if (_network.stationConnected()) {
    _attemptActive = false;
    if (!_seenConnected) {
      _seenConnected = true;
      resetBackoff();
      result.justConnected = true;
      iotlabs_log::line(_logger, "[IoTLabs] WiFi connected");
    }
    return result;
  }

  if (_seenConnected) {
    _seenConnected = false;
    result.justDisconnected = true;
    iotlabs_log::line(_logger, "[IoTLabs] WiFi disconnected");
  }

  if (_ssid.empty()) {
    result.noCredentials = true;
    return result;
  }

  if (_attemptActive) {
    if (!due(now, _attemptStartedAt + _config.wifiConnectTimeoutMs)) return result;

    _attemptActive = false;
    _network.disconnect(false);
    iotlabs_log::line(_logger, "[IoTLabs] WiFi connection attempt timed out");
    result.connectTimedOut = true;

    _nextAttemptAt = now + _backoffMs;
    _backoffMs = (_backoffMs * 2) < _config.reconnectMaxMs ? (_backoffMs * 2) : _config.reconnectMaxMs;
    return result;
  }

  if (due(now, _nextAttemptAt)) startAttempt(now, result);
  return result;
}
