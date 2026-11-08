#include "LinuxProvisioningTransport.h"

#if !defined(ARDUINO)

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace {

// Small, deliberately duplicated (~15 lines) copy of LinuxStorage's
// mkdirRecursive/defaultStateDir logic -- keeps this file self-contained
// per the task's explicit "duplication of ~10 lines is fine" guidance,
// rather than introducing a shared-header coupling for two tiny helpers.
bool mkdirRecursive(const std::string& path, ::mode_t mode) {
  if (path.empty()) return false;
  std::string partial;
  size_t pos = 0;
  if (path[0] == '/') {
    partial = "/";
    pos = 1;
  }
  while (pos <= path.size()) {
    size_t next = path.find('/', pos);
    if (next == std::string::npos) next = path.size();
    partial += path.substr(pos, next - pos);
    if (!partial.empty()) {
      if (::mkdir(partial.c_str(), mode) != 0 && errno != EEXIST) return false;
    }
    partial += "/";
    pos = next + 1;
  }
  return true;
}

std::string defaultSocketDir() {
  if (::geteuid() == 0) return "/var/lib/iotlabs";
  const char* home = std::getenv("HOME");
  if (home && home[0] != '\0') return std::string(home) + "/.local/state/iotlabs";
  return "/var/lib/iotlabs";
}

constexpr size_t kMaxLineBytes = 8192;

}  // namespace

LinuxProvisioningTransport::LinuxProvisioningTransport(std::string socketDir) : _socketDir(std::move(socketDir)) {}

LinuxProvisioningTransport::~LinuxProvisioningTransport() { stopAccessPoint(); }

bool LinuxProvisioningTransport::startAccessPoint(const char* apName, const char* apPassword) {
  if (_active) return true;
  // apPassword has no meaningful equivalent for a Unix-socket transport
  // (no over-the-air auth to protect -- filesystem permissions on the
  // socket file do that job instead); accepted per the port contract but
  // intentionally unused, not silently ignored without a note.
  (void)apPassword;

  if (_socketDir.empty()) _socketDir = defaultSocketDir();
  if (!mkdirRecursive(_socketDir, 0700)) {
    std::fprintf(stderr, "[LinuxProvisioningTransport] failed to create socket directory %s\n", _socketDir.c_str());
    return false;
  }
  ::chmod(_socketDir.c_str(), 0700);

  const std::string name = (apName && apName[0] != '\0') ? apName : "provisioning";
  _socketPath = _socketDir + "/" + name + ".sock";
  if (_socketPath.size() >= sizeof(sockaddr_un{}.sun_path)) {
    std::fprintf(stderr, "[LinuxProvisioningTransport] socket path too long: %s\n", _socketPath.c_str());
    return false;
  }

  ::unlink(_socketPath.c_str());  // Remove a stale socket file from a previous unclean shutdown.

  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return false;

  const int flags = ::fcntl(fd, F_GETFL, 0);
  ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_un addr {};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", _socketPath.c_str());

  if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return false;
  }
  ::chmod(_socketPath.c_str(), 0600);  // Same-user-only, matching LinuxStorage's secret-file permissions.

  if (::listen(fd, /*backlog=*/4) != 0) {
    ::close(fd);
    ::unlink(_socketPath.c_str());
    return false;
  }

  _listenFd = fd;
  _active = true;
  std::fprintf(stderr, "[LinuxProvisioningTransport] listening on unix socket %s\n", _socketPath.c_str());
  return true;
}

void LinuxProvisioningTransport::stopAccessPoint() {
  if (!_active) return;
  if (_listenFd >= 0) ::close(_listenFd);
  ::unlink(_socketPath.c_str());
  _listenFd = -1;
  _active = false;
}

bool LinuxProvisioningTransport::accessPointActive() const { return _active; }

void LinuxProvisioningTransport::onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) {
  _deviceInfoHandler = handler;
  _deviceInfoContext = context;
}

void LinuxProvisioningTransport::onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) {
  _wifiNetworksHandler = handler;
  _wifiNetworksContext = context;
}

void LinuxProvisioningTransport::onProvisionRequest(ProvisioningRequestHandler handler, void* context) {
  _provisionHandler = handler;
  _provisionContext = context;
}

void LinuxProvisioningTransport::poll() {
  if (!_active) return;

  const int clientFd = ::accept(_listenFd, nullptr, nullptr);
  if (clientFd < 0) return;  // EAGAIN/EWOULDBLOCK: no pending connection this tick, or a transient accept error.

  struct timeval timeout {};
  timeout.tv_sec = 2;
  ::setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  handleConnection(clientFd);
  ::close(clientFd);
}

void LinuxProvisioningTransport::handleConnection(int clientFd) {
  std::string line;
  char chunk[512];
  bool sawNewline = false;
  while (line.size() < kMaxLineBytes) {
    const ssize_t n = ::recv(clientFd, chunk, sizeof(chunk), 0);
    if (n <= 0) break;  // Peer closed, or SO_RCVTIMEO fired (a slow/stuck client just gets dropped).
    line.append(chunk, static_cast<size_t>(n));
    const size_t nl = line.find('\n');
    if (nl != std::string::npos) {
      line.resize(nl);
      sawNewline = true;
      break;
    }
  }
  if (!sawNewline) {
    sendResponse(clientFd, 400,
                 R"({"error":{"code":"BAD_REQUEST","message":"Expected a single newline-terminated request line"}})");
    return;
  }
  if (!line.empty() && line.back() == '\r') line.pop_back();

  const size_t sp1 = line.find(' ');
  if (sp1 == std::string::npos) {
    sendResponse(clientFd, 400, R"({"error":{"code":"BAD_REQUEST","message":"Missing path"}})");
    return;
  }
  const std::string verb = line.substr(0, sp1);
  const size_t sp2 = line.find(' ', sp1 + 1);
  std::string path;
  std::string body;
  if (sp2 == std::string::npos) {
    path = line.substr(sp1 + 1);
  } else {
    path = line.substr(sp1 + 1, sp2 - sp1 - 1);
    body = line.substr(sp2 + 1);
  }

  dispatch(verb, path, body, clientFd);
}

void LinuxProvisioningTransport::dispatch(const std::string& verb, const std::string& path, const std::string& body,
                                           int clientFd) {
  (void)verb;  // The path alone already disambiguates the 3 known endpoints for this local protocol.

  ProvisioningRequestHandler handler = nullptr;
  void* context = nullptr;
  if (path == "/device-info") {
    handler = _deviceInfoHandler;
    context = _deviceInfoContext;
  } else if (path == "/wifi/networks") {
    handler = _wifiNetworksHandler;
    context = _wifiNetworksContext;
  } else if (path == "/provision") {
    handler = _provisionHandler;
    context = _provisionContext;
  } else {
    sendResponse(clientFd, 404, R"({"error":{"code":"NOT_FOUND","message":"Unknown provisioning endpoint"}})");
    return;
  }

  if (!handler) {
    sendResponse(clientFd, 500, R"({"error":{"code":"NO_HANDLER","message":"Route not wired"}})");
    return;
  }

  IoTLabsHttpExchange exchange;
  exchange.requestBody = body.c_str();
  handler(context, exchange);
  sendResponse(clientFd, exchange.responseStatus, exchange.responseBody);
}

void LinuxProvisioningTransport::sendResponse(int clientFd, int status, const std::string& body) {
  const std::string line = std::to_string(status) + " " + body + "\n";
  size_t sent = 0;
  while (sent < line.size()) {
    const ssize_t n = ::send(clientFd, line.data() + sent, line.size() - sent, 0);
    if (n <= 0) break;  // Peer gone or SO_SNDTIMEO fired -- nothing more we can do.
    sent += static_cast<size_t>(n);
  }
}

#endif  // !ARDUINO
