#include "Stm32ArduinoMqttTransport.h"

#if defined(ARDUINO_ARCH_STM32)

Stm32ArduinoMqttTransport* Stm32ArduinoMqttTransport::_activeInstance = nullptr;

Stm32ArduinoMqttTransport::Stm32ArduinoMqttTransport() : _mqtt(_tcpClient) {
  _activeInstance = this;
  _mqtt.setCallback(&Stm32ArduinoMqttTransport::pubSubThunk);
}

void Stm32ArduinoMqttTransport::configure(const MqttTransportConfig& config) {
  // tlsInsecure/rootCA are intentionally not applied: see the header's
  // KNOWN LIMITATION note. Loud, not silent, about it here too, since
  // configure() is the one call site that actually receives rootCA.
  if (!config.tlsInsecure || config.rootCA) {
    Serial.println(
        "[Stm32ArduinoMqttTransport] WARNING: connecting in PLAINTEXT TCP (no TLS-capable Client available on "
        "this STM32Ethernet adapter) even though a rootCA/TLS configuration was supplied.");
  }

  _mqtt.setServer(config.host, config.port);
  _mqtt.setKeepAlive(config.keepAliveSeconds);
  _mqtt.setSocketTimeout(config.socketTimeoutSeconds);
  _mqtt.setBufferSize(config.bufferBytes);
}

void Stm32ArduinoMqttTransport::setMessageCallback(MqttMessageCallback callback, void* context) {
  _callback = callback;
  _context = context;
}

bool Stm32ArduinoMqttTransport::connect(const MqttConnectionOptions& options) {
  return _mqtt.connect(options.clientId, options.username, options.password, options.willTopic, 0,
                        options.willRetain, options.willPayload);
}

void Stm32ArduinoMqttTransport::disconnect() { _mqtt.disconnect(); }

bool Stm32ArduinoMqttTransport::connected() const { return _mqtt.connected(); }

bool Stm32ArduinoMqttTransport::subscribe(const char* topic) { return _mqtt.subscribe(topic); }

bool Stm32ArduinoMqttTransport::publish(const char* topic, const uint8_t* payload, size_t length, bool retained) {
  return _mqtt.publish(topic, payload, length, retained);
}

void Stm32ArduinoMqttTransport::poll() { _mqtt.loop(); }

int Stm32ArduinoMqttTransport::lastState() const { return _mqtt.state(); }

void Stm32ArduinoMqttTransport::pubSubThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (!_activeInstance || !_activeInstance->_callback) return;
  _activeInstance->_callback(_activeInstance->_context, topic, payload, length);
}

#endif  // ARDUINO_ARCH_STM32
