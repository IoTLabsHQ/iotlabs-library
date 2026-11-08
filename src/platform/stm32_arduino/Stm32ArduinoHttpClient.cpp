#include "Stm32ArduinoHttpClient.h"

#if defined(ARDUINO_ARCH_STM32)

#include <ArduinoHttpClient.h>
// See Stm32ProvisioningTransport.h for why LwIP.h must precede STM32Ethernet.h.
#include <LwIP.h>
#include <STM32Ethernet.h>

#include <cstdlib>
#include <string>

namespace {

// Splits "[scheme://]host[:port][/path]" into its parts. The scheme (if
// any) is discarded entirely: this adapter only ever speaks plain HTTP (see
// the header's KNOWN LIMITATION note), so an "https://" prefix does not
// change behavior, it is simply ignored rather than honored or rejected.
void parseUrl(const char* url, std::string& host, uint16_t& port, std::string& path) {
  const std::string s(url ? url : "");
  const size_t schemeEnd = s.find("://");
  const size_t start = (schemeEnd == std::string::npos) ? 0 : schemeEnd + 3;
  const size_t pathStart = s.find('/', start);

  const std::string hostPort =
      s.substr(start, pathStart == std::string::npos ? std::string::npos : pathStart - start);
  path = (pathStart == std::string::npos) ? "/" : s.substr(pathStart);
  if (path.empty()) path = "/";

  const size_t colon = hostPort.find(':');
  if (colon == std::string::npos) {
    host = hostPort;
    port = 80;
  } else {
    host = hostPort.substr(0, colon);
    port = static_cast<uint16_t>(strtoul(hostPort.substr(colon + 1).c_str(), nullptr, 10));
    if (port == 0) port = 80;
  }
}

}  // namespace

bool Stm32ArduinoHttpClient::postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure,
                                       const char* rootCA, IoTLabsHttpResponse& outResponse) {
  (void)tlsInsecure;
  (void)rootCA;
  // Loud, not silent: STM32Ethernet ships no TLS-capable Client for this
  // core/library combination (see the header's KNOWN LIMITATION note), so
  // every activation POST really does go out as plaintext HTTP regardless
  // of what the caller asked for.
  Serial.println(
      "[Stm32ArduinoHttpClient] WARNING: sending PLAINTEXT HTTP (no TLS-capable Client available on this "
      "STM32Ethernet adapter) even though the requested URL may be https://");

  std::string host;
  uint16_t port = 80;
  std::string path;
  parseUrl(url, host, port, path);
  if (host.empty()) return false;

  EthernetClient tcpClient;
  HttpClient http(tcpClient, host.c_str(), port);
  http.setHttpResponseTimeout(timeoutMs);

  const int postResult = http.post(path.c_str(), "application/json", jsonBody ? jsonBody : "");
  if (postResult != HTTP_SUCCESS) {
    http.stop();
    return false;
  }

  outResponse.statusCode = http.responseStatusCode();
  const String body = http.responseBody();
  outResponse.body = std::string(body.c_str());
  http.stop();
  return true;
}

#endif  // ARDUINO_ARCH_STM32
