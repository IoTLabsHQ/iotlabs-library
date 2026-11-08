#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "../../core/ports/IMqttTransport.h"

class Esp32ArduinoMqttTransport : public IMqttTransport {
 public:
  Esp32ArduinoMqttTransport();

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
  WiFiClientSecure _secureClient;
  // PubSubClient::connected()/state() are not const-qualified upstream even
  // though they don't logically mutate the connection; mutable lets this
  // adapter still satisfy IMqttTransport's const query methods.
  mutable PubSubClient _mqtt;

  MqttMessageCallback _callback = nullptr;
  void* _context = nullptr;

  // PubSubClient::setCallback only accepts a plain function pointer (no
  // userdata slot), so a single static "which instance is live" pointer is
  // required to route back into member state — same pattern the original
  // monolith used for IoTLabs::mqttCallbackThunk.
  static Esp32ArduinoMqttTransport* _activeInstance;
  static void pubSubThunk(char* topic, uint8_t* payload, unsigned int length);
};

#endif  // ARDUINO_ARCH_ESP32
