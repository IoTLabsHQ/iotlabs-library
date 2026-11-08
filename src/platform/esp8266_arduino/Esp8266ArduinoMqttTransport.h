#pragma once

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board, and headers get pulled in transitively even where the
// .cpp using them is guarded out -- so this whole declaration is an
// ESP8266-only no-op when parsed for another core. (PubSubClient.h itself
// is portable and already used by the ESP32 adapter; it's
// WiFiClientSecureBearSSL.h that is ESP8266-specific.)
#if defined(ARDUINO_ARCH_ESP8266)

#include <PubSubClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <memory>

#include "../../core/ports/IMqttTransport.h"

class Esp8266ArduinoMqttTransport : public IMqttTransport {
 public:
  Esp8266ArduinoMqttTransport();

  void configure(const MqttTransportConfig& config) override;
  void setMessageCallback(MqttMessageCallback callback, void* context) override;

  bool connect(const MqttConnectionOptions& options) override;
  void disconnect() override;
  bool connected() const override;

  bool subscribe(const char* topic) override;
  bool publish(const char* topic, const uint8_t* payload, size_t length, bool retained) override;

  void poll() override;
  int lastState() const override;

 private:
  BearSSL::WiFiClientSecure _secureClient;
  // PubSubClient::connected()/state() are not const-qualified upstream even
  // though they don't logically mutate the connection; mutable lets this
  // adapter still satisfy IMqttTransport's const query methods.
  mutable PubSubClient _mqtt;

  // ESP8266's BearSSL::WiFiClientSecure needs a parsed BearSSL::X509List
  // trust anchor (see Esp8266ArduinoHttpClient.cpp) instead of ESP32's
  // setCACert(const char*); owned here so it outlives every connect() call
  // configure() precedes, not just one request.
  std::unique_ptr<BearSSL::X509List> _trustAnchor;

  MqttMessageCallback _callback = nullptr;
  void* _context = nullptr;

  // PubSubClient::setCallback only accepts a plain function pointer (no
  // userdata slot), so a single static "which instance is live" pointer is
  // required to route back into member state -- same pattern as
  // Esp32ArduinoMqttTransport::pubSubThunk.
  static Esp8266ArduinoMqttTransport* _activeInstance;
  static void pubSubThunk(char* topic, uint8_t* payload, unsigned int length);
};

#endif  // ARDUINO_ARCH_ESP8266
