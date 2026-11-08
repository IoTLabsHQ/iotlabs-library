#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include "../../core/ports/IHttpClient.h"

// KNOWN LIMITATION (see IoTLabs internal design notes):
// STM32Ethernet's EthernetClient is plain TCP only — this library version
// ships no TLS-capable Client for the Ethernet path (unlike ESP32's
// WiFiClientSecure). tlsInsecure/rootCA are accepted for IHttpClient
// interface compatibility but are NOT honored: every request is sent over
// plain, unencrypted HTTP regardless of what the caller passes. This
// adapter does not pretend otherwise — see the .cpp for the loud runtime
// log warning on every call.
class Stm32ArduinoHttpClient : public IHttpClient {
 public:
  bool postJson(const char* url, const char* jsonBody, uint32_t timeoutMs, bool tlsInsecure, const char* rootCA,
                IoTLabsHttpResponse& outResponse) override;
};

#endif  // ARDUINO_ARCH_STM32
