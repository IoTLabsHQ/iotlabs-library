#include "Esp32ArduinoMqttTransport.h"

#if defined(ARDUINO_ARCH_ESP32)

Esp32ArduinoMqttTransport* Esp32ArduinoMqttTransport::_activeInstance = nullptr;

Esp32ArduinoMqttTransport::Esp32ArduinoMqttTransport() : _mqtt(_secureClient) {
  _activeInstance = this;
  _mqtt.setCallback(&Esp32ArduinoMqttTransport::pubSubThunk);
}

void Esp32ArduinoMqttTransport::configure(const MqttTransportConfig& config) {
  if (config.tlsInsecure || !config.rootCA) {
    _secureClient.setInsecure();
  } else {
    _secureClient.setCACert(config.rootCA);
  }

  _mqtt.setServer(config.host, config.port);
  _mqtt.setKeepAlive(config.keepAliveSeconds);
  _mqtt.setSocketTimeout(config.socketTimeoutSeconds);
  _mqtt.setBufferSize(config.bufferBytes);
}

void Esp32ArduinoMqttTransport::setMessageCallback(MqttMessageCallback callback, void* context) {
  _callback = callback;
  _context = context;
}

bool Esp32ArduinoMqttTransport::connect(const MqttConnectionOptions& options) {
  return _mqtt.connect(options.clientId, options.username, options.password, options.willTopic, 0,
                        options.willRetain, options.willPayload);
}

void Esp32ArduinoMqttTransport::disconnect() { _mqtt.disconnect(); }

bool Esp32ArduinoMqttTransport::connected() const { return _mqtt.connected(); }

bool Esp32ArduinoMqttTransport::subscribe(const char* topic) { return _mqtt.subscribe(topic); }

bool Esp32ArduinoMqttTransport::publish(const char* topic, const uint8_t* payload, size_t length, bool retained) {
  return _mqtt.publish(topic, payload, length, retained);
}

void Esp32ArduinoMqttTransport::poll() { _mqtt.loop(); }

int Esp32ArduinoMqttTransport::lastState() const { return _mqtt.state(); }

void Esp32ArduinoMqttTransport::pubSubThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (!_activeInstance || !_activeInstance->_callback) return;
  _activeInstance->_callback(_activeInstance->_context, topic, payload, length);
}

#endif  // ARDUINO_ARCH_ESP32
