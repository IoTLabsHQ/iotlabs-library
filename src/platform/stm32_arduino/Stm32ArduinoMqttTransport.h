#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include <PubSubClient.h>
// See Stm32ProvisioningTransport.h for why LwIP.h must precede STM32Ethernet.h.
#include <LwIP.h>
#include <STM32Ethernet.h>

#include "../../core/ports/IMqttTransport.h"

// KNOWN LIMITATION (see IoTLabs internal design notes):
// same as Stm32ArduinoHttpClient — STM32Ethernet's EthernetClient is plain
// TCP only, so this transport connects in PLAINTEXT regardless of
// MqttTransportConfig::tlsInsecure/rootCA. PubSubClient itself is
// transport-agnostic (works over any Client&, the same library the ESP32
// adapter uses over WiFiClientSecure), so swapping in a TLS-capable Client
// later is a contained change to this one file if/when a TLS-capable
// Ethernet Client becomes available for this core.
class Stm32ArduinoMqttTransport : public IMqttTransport {
 public:
  Stm32ArduinoMqttTransport();

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
  EthernetClient _tcpClient;
  // PubSubClient::connected()/state() are not const-qualified upstream even
  // though they don't logically mutate the connection; mutable lets this
  // adapter still satisfy IMqttTransport's const query methods (same
  // rationale as Esp32ArduinoMqttTransport).
  mutable PubSubClient _mqtt;

  MqttMessageCallback _callback = nullptr;
  void* _context = nullptr;

  // PubSubClient::setCallback only accepts a plain function pointer (no
  // userdata slot), so a single static "which instance is live" pointer is
  // required to route back into member state — same pattern as
  // Esp32ArduinoMqttTransport.
  static Stm32ArduinoMqttTransport* _activeInstance;
  static void pubSubThunk(char* topic, uint8_t* payload, unsigned int length);
};

#endif  // ARDUINO_ARCH_STM32
