#pragma once

#if !defined(ARDUINO)

#include <string>

#include "../../core/ports/IProvisioningTransport.h"

// "CLI/file/other Linux-appropriate transport" (05_LINUX_RASPBERRY_PI.md):
// a Linux service does not run a SoftAP + local HTTP portal like ESP32.
// This adapter picks ONE concrete, real, working mechanism -- a Unix domain
// socket exposing a tiny line-based JSON protocol -- rather than stubbing
// the port out as permanently inactive. Rationale (also recorded in
// IoTLabs internal design notes):
//
//   - A Unix domain socket is filesystem-permission-securable out of the
//     box (0600, same user only), needs zero extra dependencies (raw POSIX
//     sockets), and a companion CLI/provisioning tool on the same host can
//     talk to it trivially with nothing more than `socat`/`nc -U` or a
//     few lines of any language's socket API.
//   - A full local HTTP server (the filesystem-watch or minimal HTTP
//     options the task also allowed) would either add a parsing surface
//     this candidate doesn't need, or a polling-latency tradeoff a socket
//     avoids entirely.
//
// Wire protocol (intentionally minimal -- this is a local, same-host,
// single-client control channel, not a public API):
//   Request  (client -> server, one line): "<VERB> <PATH>[ <BODY>]\n"
//     VERB is "GET" or "POST", PATH is "/device-info", "/wifi/networks", or
//     "/provision". BODY (POST /provision only) is the request's raw JSON,
//     assumed compact/single-line (ArduinoJson's default serializeJson()
//     output has no embedded newlines).
//   Response (server -> client, one line): "<STATUS> <BODY>\n"
//     STATUS is the numeric HTTP-style status IoTLabsHttpExchange.responseStatus
//     already uses; BODY is the raw JSON response body.
//
// startAccessPoint()/stopAccessPoint()/accessPointActive() map to "the
// socket is listening" on/off rather than a literal WiFi AP -- the port is
// intentionally abstract enough for that (05_LINUX_RASPBERRY_PI.md: AP
// name/password are accepted but only apName is used, to derive the socket
// path, since a Unix socket has no equivalent of an AP password).
class LinuxProvisioningTransport : public IProvisioningTransport {
 public:
  // socketDir: overrides the default directory the socket file is created
  // in. Empty (default) picks LinuxStorage::defaultStateDir()'s sibling
  // convention at startAccessPoint() time.
  explicit LinuxProvisioningTransport(std::string socketDir = std::string());
  ~LinuxProvisioningTransport() override;

  bool startAccessPoint(const char* apName, const char* apPassword) override;
  void stopAccessPoint() override;
  bool accessPointActive() const override;

  void onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) override;
  void onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) override;
  void onProvisionRequest(ProvisioningRequestHandler handler, void* context) override;

  // Non-blocking accept() of at most one new client per call, then a
  // bounded-timeout (SO_RCVTIMEO/SO_SNDTIMEO, a couple seconds) synchronous
  // read/dispatch/write/close for that one connection -- a deliberate
  // tradeoff, documented in BOARD_QUALIFICATION.md, so a slow/malformed
  // client can only ever stall one poll() call briefly, never the engine
  // loop indefinitely.
  void poll() override;

  // Full path of the socket file, valid once startAccessPoint() succeeds.
  std::string socketPathForTest() const { return _socketPath; }

 private:
  std::string _socketDir;
  std::string _socketPath;
  int _listenFd = -1;
  bool _active = false;

  ProvisioningRequestHandler _deviceInfoHandler = nullptr;
  void* _deviceInfoContext = nullptr;
  ProvisioningRequestHandler _wifiNetworksHandler = nullptr;
  void* _wifiNetworksContext = nullptr;
  ProvisioningRequestHandler _provisionHandler = nullptr;
  void* _provisionContext = nullptr;

  void handleConnection(int clientFd);
  void dispatch(const std::string& verb, const std::string& path, const std::string& body, int clientFd);
  static void sendResponse(int clientFd, int status, const std::string& body);
};

#endif  // !ARDUINO
