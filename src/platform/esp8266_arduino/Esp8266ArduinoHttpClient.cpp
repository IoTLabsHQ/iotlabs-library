#include "Esp8266ArduinoHttpClient.h"

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board; <ESP8266HTTPClient.h>/<WiFiClientSecureBearSSL.h> only
// exist on the ESP8266 core.
#if defined(ARDUINO_ARCH_ESP8266)

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <cstring>
#include <memory>

bool Esp8266ArduinoHttpClient::postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure,
                                         const char* rootCA, IoTLabsHttpResponse& outResponse) {
  BearSSL::WiFiClientSecure client;
  // ESP8266's BearSSL::WiFiClientSecure has no ESP32-style setCACert(const
  // char*); a PEM root CA is parsed into a BearSSL::X509List trust anchor
  // instead. Only constructed when actually needed (parsing costs heap),
  // and kept alive for the whole call since client.setTrustAnchors() only
  // stores a pointer -- safe here because postJson() is a synchronous,
  // single-shot request/response exchange.
  std::unique_ptr<BearSSL::X509List> ca;
  if (tlsInsecure || !rootCA) {
    client.setInsecure();
  } else {
    ca.reset(new BearSSL::X509List(rootCA));
    client.setTrustAnchors(ca.get());
  }

  HTTPClient http;
  // HTTPClient::setTimeout() only takes uint16_t on this core (unlike
  // ESP32's uint32_t overload); clamp instead of silently truncating.
  const uint16_t clampedTimeoutMs = timeoutMs > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(timeoutMs);
  http.setTimeout(clampedTimeoutMs);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");

  const int statusCode = http.POST(String(jsonBody));
  outResponse.statusCode = statusCode;
  outResponse.body = statusCode > 0 ? std::string(http.getString().c_str()) : std::string();
  http.end();
  return true;
}

#endif  // ARDUINO_ARCH_ESP8266
