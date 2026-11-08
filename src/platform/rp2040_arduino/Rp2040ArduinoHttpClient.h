#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include "../../core/ports/IHttpClient.h"

class Rp2040ArduinoHttpClient : public IHttpClient {
 public:
  bool postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure, const char* rootCA,
                IoTLabsHttpResponse& outResponse) override;
};

#endif  // ARDUINO_ARCH_RP2040
