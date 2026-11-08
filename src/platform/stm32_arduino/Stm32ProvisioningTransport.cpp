#include "Stm32ProvisioningTransport.h"

#if defined(ARDUINO_ARCH_STM32)

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

constexpr uint32_t kLineTimeoutMs = 3000;
constexpr size_t kMaxHeaderLineLen = 512;
constexpr size_t kMaxBodyLen = 4096;
constexpr unsigned long kDhcpTimeoutMs = 15000;
constexpr unsigned long kDhcpResponseTimeoutMs = 4000;

// Reads one CRLF- or LF-terminated line (trailing \r stripped). Returns
// false on timeout or a disconnected client; bounds line length so a
// misbehaving/malicious client can't grow this unbounded.
bool readLine(EthernetClient& client, std::string& outLine, uint32_t timeoutMs) {
  outLine.clear();
  const uint32_t deadline = millis() + timeoutMs;
  while (true) {
    if (client.available()) {
      const int c = client.read();
      if (c < 0) continue;
      if (c == '\n') {
        if (!outLine.empty() && outLine.back() == '\r') outLine.pop_back();
        return true;
      }
      if (outLine.size() < kMaxHeaderLineLen) outLine += static_cast<char>(c);
    } else if (!client.connected()) {
      return false;
    } else if (millis() > deadline) {
      return false;
    }
  }
}

// Case-insensitive "does this header line start with 'Content-Length:'"
// check, hand-rolled instead of strncasecmp (not guaranteed present across
// every newlib-nano configuration this core might use).
bool isContentLengthHeader(const std::string& line) {
  static const char* kPrefix = "content-length:";
  const size_t n = std::strlen(kPrefix);
  if (line.size() < n) return false;
  for (size_t i = 0; i < n; ++i) {
    if (std::tolower(static_cast<unsigned char>(line[i])) != kPrefix[i]) return false;
  }
  return true;
}

}  // namespace

Stm32ProvisioningTransport::Stm32ProvisioningTransport() : _server(80) {}

bool Stm32ProvisioningTransport::startAccessPoint(const char* apName, const char* apPassword) {
  // apName/apPassword are unused: see the header's KNOWN ARCHITECTURE
  // DEVIATION note — there is no SoftAP to name/secure on a wired link.
  (void)apName;
  (void)apPassword;
  if (_active) return true;

  const IPAddress ip = Ethernet.localIP();
  const bool alreadyUp = (ip[0] || ip[1] || ip[2] || ip[3]) && Ethernet.linkStatus() != LinkOFF;
  if (!alreadyUp) {
    if (Ethernet.begin(kDhcpTimeoutMs, kDhcpResponseTimeoutMs) != 1) return false;
  }

  _server.begin();
  _active = true;
  return true;
}

void Stm32ProvisioningTransport::stopAccessPoint() {
  if (!_active) return;
  _server.end();
  _active = false;
  // Deliberately does not tear down the underlying Ethernet/DHCP link
  // (there is nothing AP-specific to tear down on a wired PHY): matches
  // Stm32ArduinoNetwork::disconnect()'s equivalent no-op reasoning.
}

bool Stm32ProvisioningTransport::accessPointActive() const { return _active; }

void Stm32ProvisioningTransport::onDeviceInfoRequest(ProvisioningRequestHandler handler, void* context) {
  _deviceInfoHandler = handler;
  _deviceInfoContext = context;
}

void Stm32ProvisioningTransport::onWifiNetworksRequest(ProvisioningRequestHandler handler, void* context) {
  _wifiNetworksHandler = handler;
  _wifiNetworksContext = context;
}

void Stm32ProvisioningTransport::onProvisionRequest(ProvisioningRequestHandler handler, void* context) {
  _provisionHandler = handler;
  _provisionContext = context;
}

void Stm32ProvisioningTransport::poll() {
  // Services the LwIP stack even while not actively provisioning, mirroring
  // Stm32ArduinoNetwork::stationConnected()'s reasoning: IProvisioningTransport::poll()
  // is called every loop() tick regardless of _active, so it is as good a
  // place as INetwork::stationConnected() to keep the stack serviced.
  Ethernet.schedule();
  if (!_active) return;

  EthernetClient client = _server.available();
  if (!client) return;

  handleClient(client);
  client.stop();
}

void Stm32ProvisioningTransport::handleClient(EthernetClient& client) {
  std::string requestLine;
  if (!readLine(client, requestLine, kLineTimeoutMs) || requestLine.empty()) return;

  const size_t sp1 = requestLine.find(' ');
  const size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : requestLine.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) return;
  const std::string method = requestLine.substr(0, sp1);
  const std::string path = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);

  size_t contentLength = 0;
  std::string headerLine;
  while (readLine(client, headerLine, kLineTimeoutMs)) {
    if (headerLine.empty()) break;  // blank line: end of headers.
    if (isContentLengthHeader(headerLine)) {
      contentLength = static_cast<size_t>(std::strtoul(headerLine.c_str() + std::strlen("content-length:"), nullptr, 10));
    }
  }
  if (contentLength > kMaxBodyLen) contentLength = kMaxBodyLen;

  _requestBody.clear();
  _requestBody.reserve(contentLength);
  const uint32_t bodyDeadline = millis() + kLineTimeoutMs;
  while (_requestBody.size() < contentLength) {
    if (client.available()) {
      const int c = client.read();
      if (c >= 0) _requestBody += static_cast<char>(c);
    } else if (!client.connected() || millis() > bodyDeadline) {
      break;
    }
  }

  if (method == "GET" && path == "/device-info") {
    dispatch(client, _deviceInfoHandler, _deviceInfoContext, _requestBody);
  } else if (method == "GET" && path == "/wifi/networks") {
    dispatch(client, _wifiNetworksHandler, _wifiNetworksContext, _requestBody);
  } else if (method == "POST" && path == "/provision") {
    dispatch(client, _provisionHandler, _provisionContext, _requestBody);
  } else {
    sendNotFound(client);
  }
}

void Stm32ProvisioningTransport::dispatch(EthernetClient& client, ProvisioningRequestHandler handler, void* context,
                                           const std::string& body) {
  if (!handler) {
    static const char* kMsg = "{\"error\":{\"code\":\"NO_HANDLER\",\"message\":\"Route not wired\"}}";
    client.print("HTTP/1.1 500 Internal Server Error\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: ");
    client.print(static_cast<unsigned long>(std::strlen(kMsg)));
    client.print("\r\n\r\n");
    client.print(kMsg);
    return;
  }

  IoTLabsHttpExchange exchange;
  exchange.requestBody = body.c_str();
  handler(context, exchange);

  client.print("HTTP/1.1 ");
  client.print(exchange.responseStatus);
  client.print(" OK\r\n");
  client.print("Content-Type: ");
  client.print(exchange.responseContentType);
  client.print("\r\n");
  client.print("Connection: close\r\n");
  const size_t len = std::strlen(exchange.responseBody);
  client.print("Content-Length: ");
  client.print(static_cast<unsigned long>(len));
  client.print("\r\n\r\n");
  client.write(reinterpret_cast<const uint8_t*>(exchange.responseBody), len);
}

void Stm32ProvisioningTransport::sendNotFound(EthernetClient& client) {
  static const char* kMsg = "{\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Unknown provisioning endpoint\"}}";
  client.print("HTTP/1.1 404 Not Found\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(static_cast<unsigned long>(std::strlen(kMsg)));
  client.print("\r\n\r\n");
  client.print(kMsg);
}

#endif  // ARDUINO_ARCH_STM32
