#pragma once

#include <cstdint>
#include <string>

#include "Log.h"
#include "ports/IClock.h"
#include "ports/ILogger.h"
#include "ports/INetwork.h"

// Wi-Fi station connect/retry/backoff state machine, ported 1:1 from the
// original monolithic serviceWiFi()/startWifiAttempt() so RT-01..03 timing
// behavior is preserved exactly.
class ConnectionManager {
 public:
  struct Config {
    uint32_t reconnectMinMs = 1000;
    uint32_t reconnectMaxMs = 30000;
    uint32_t wifiConnectTimeoutMs = 15000;
  };

  struct TickResult {
    bool justConnected = false;
    bool justDisconnected = false;
    bool connectTimedOut = false;
    bool attemptStarted = false;
    bool noCredentials = false;
  };

  ConnectionManager(INetwork& network, IClock& clock, ILogger* logger);

  void configure(const Config& config);
  void setCredentials(const std::string& ssid, const std::string& password);
  // Atomically swaps in credentials just accepted via provisioning and
  // schedules the next attempt, discarding any in-flight attempt state so a
  // stale connect against the old credentials can't race the new one.
  void applyNewCredentials(const std::string& ssid, const std::string& password, uint32_t nextAttemptAt);
  void clearCredentials();
  bool hasCredentials() const { return !_ssid.empty(); }

  void resetBackoff();
  void requestImmediateAttempt(uint32_t now);

  TickResult tick(uint32_t now);
  bool connected() const { return _network.stationConnected(); }
  uint32_t backoffMs() const { return _backoffMs; }

 private:
  INetwork& _network;
  IClock& _clock;
  ILogger* _logger;
  Config _config;

  std::string _ssid;
  std::string _password;

  bool _seenConnected = false;
  bool _attemptActive = false;
  uint32_t _attemptStartedAt = 0;
  uint32_t _nextAttemptAt = 0;
  uint32_t _backoffMs = 1000;

  void startAttempt(uint32_t now, TickResult& result);
};
