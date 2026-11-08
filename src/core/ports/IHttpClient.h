#pragma once

#include <cstdint>
#include <string>

struct IoTLabsHttpResponse {
  int statusCode = 0;
  std::string body;
};

// One-shot HTTPS POST used for activation. Deliberately not a general HTTP
// client: the core only ever needs a single request/response exchange.
class IHttpClient {
 public:
  virtual ~IHttpClient() = default;

  virtual bool postJson(const char* url,
                        const char* jsonBody,
                        uint32_t timeoutMs,
                        bool tlsInsecure,
                        const char* rootCA,
                        IoTLabsHttpResponse& outResponse) = 0;
};
