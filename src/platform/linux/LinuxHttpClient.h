#pragma once

#if !defined(ARDUINO)

#include "../../core/ports/IHttpClient.h"

// One-shot HTTPS POST via libcurl (confirmed available on this dev machine
// via curl-config/pkg-config; on real Linux/RPi it's the libcurl4-openssl-
// dev apt package -- see IoTLabs internal design notes). See
// LinuxHttpClient.cpp for the CAINFO_BLOB-vs-temp-file decision for the
// `rootCA` PEM-content parameter.
class LinuxHttpClient : public IHttpClient {
 public:
  LinuxHttpClient();
  ~LinuxHttpClient() override;

  bool postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure, const char* rootCA,
                IoTLabsHttpResponse& outResponse) override;
};

#endif  // !ARDUINO
