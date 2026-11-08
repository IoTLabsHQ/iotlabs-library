#pragma once

#include "../../core/ports/IHttpClient.h"

class Esp8266ArduinoHttpClient : public IHttpClient {
 public:
  bool postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure, const char* rootCA,
                IoTLabsHttpResponse& outResponse) override;
};
