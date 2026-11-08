#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "../../core/ports/IHttpClient.h"

class Esp32ArduinoHttpClient : public IHttpClient {
 public:
  bool postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure, const char* rootCA,
                IoTLabsHttpResponse& outResponse) override;
};

#endif  // ARDUINO_ARCH_ESP32
