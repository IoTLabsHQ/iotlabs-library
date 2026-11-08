#pragma once

#include <cstddef>
#include <cstdint>

struct MqttTransportConfig {
  const char* host = nullptr;
  uint16_t port = 8883;
  bool tlsInsecure = false;
  const char* rootCA = nullptr;
  uint16_t keepAliveSeconds = 60;
  uint16_t socketTimeoutSeconds = 10;
  uint16_t bufferBytes = 1536;
};

struct MqttConnectionOptions {
  const char* clientId = nullptr;
  const char* username = nullptr;
  const char* password = nullptr;
  const char* willTopic = nullptr;
  const char* willPayload = nullptr;
  bool willRetain = false;
};

using MqttMessageCallback = void (*)(void* context,
                                      const char* topic,
                                      const uint8_t* payload,
                                      size_t length);

// Narrow MQTT/TLS capability. Signature mirrors IoTLabs internal design notes
class IMqttTransport {
 public:
  virtual ~IMqttTransport() = default;

  virtual void configure(const MqttTransportConfig& config) = 0;
  virtual void setMessageCallback(MqttMessageCallback callback, void* context) = 0;

  virtual bool connect(const MqttConnectionOptions& options) = 0;
  virtual void disconnect() = 0;
  virtual bool connected() const = 0;

  virtual bool subscribe(const char* topic) = 0;
  virtual bool publish(const char* topic, const uint8_t* payload, size_t length, bool retained) = 0;

  virtual void poll() = 0;
  virtual int lastState() const = 0;
};
