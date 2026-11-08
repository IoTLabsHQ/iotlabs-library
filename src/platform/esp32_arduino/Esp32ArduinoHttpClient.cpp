#include "Esp32ArduinoHttpClient.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <cstring>

namespace {
// HTTPClient::begin() picks its transport from the WiFiClient subtype passed
// in, not from the URL scheme — passing a WiFiClientSecure always attempts a
// TLS handshake even for a plain "http://" URL, which fails outright against
// a plain-HTTP server (e.g. a local dev Device Platform with no TLS
// terminator) instead of falling back to plain HTTP. Branch on scheme so
// "http://" genuinely uses plain WiFiClient.
bool isHttps(const char* url) {
  return url && strncmp(url, "https://", 8) == 0;
}
}  // namespace

bool Esp32ArduinoHttpClient::postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure,
                                       const char* rootCA, IoTLabsHttpResponse& outResponse) {
  // Both declared at function scope (not inside the if/else below) because
  // HTTPClient::begin() keeps a reference to whichever one is passed in —
  // it must stay alive through http.POST()/http.end(), not just through begin().
  WiFiClient plainClient;
  WiFiClientSecure secureClient;

  HTTPClient http;
  http.setTimeout(timeoutMs);

  bool began = false;
  if (isHttps(url)) {
    if (tlsInsecure || !rootCA) {
      secureClient.setInsecure();
    } else {
      secureClient.setCACert(rootCA);
    }
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(plainClient, url);
  }
  if (!began) return false;

  http.addHeader("Content-Type", "application/json");

  const int statusCode = http.POST(String(jsonBody));
  outResponse.statusCode = statusCode;
  outResponse.body = statusCode > 0 ? std::string(http.getString().c_str()) : std::string();
  http.end();
  return true;
}

#endif  // ARDUINO_ARCH_ESP32
