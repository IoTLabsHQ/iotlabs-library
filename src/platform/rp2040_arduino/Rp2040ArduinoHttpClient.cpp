#include "Rp2040ArduinoHttpClient.h"

#if defined(ARDUINO_ARCH_RP2040)

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <cstring>

bool Rp2040ArduinoHttpClient::postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure,
                                        const char* rootCA, IoTLabsHttpResponse& outResponse) {
  WiFiClientSecure client;
  if (tlsInsecure || !rootCA) {
    client.setInsecure();
  } else {
    client.setCACert(rootCA);
  }

  HTTPClient http;
  http.setTimeout(timeoutMs);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");

  const int statusCode = http.POST(String(jsonBody));
  outResponse.statusCode = statusCode;
  outResponse.body = statusCode > 0 ? std::string(http.getString().c_str()) : std::string();
  http.end();
  return true;
}

#endif  // ARDUINO_ARCH_RP2040
