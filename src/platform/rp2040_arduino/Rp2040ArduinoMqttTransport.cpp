#include "Rp2040ArduinoMqttTransport.h"

#if defined(ARDUINO_ARCH_RP2040)

Rp2040ArduinoMqttTransport* Rp2040ArduinoMqttTransport::_activeInstance = nullptr;

Rp2040ArduinoMqttTransport::Rp2040ArduinoMqttTransport() : _mqtt(_secureClient) {
  _activeInstance = this;
  _mqtt.setCallback(&Rp2040ArduinoMqttTransport::pubSubThunk);
}

void Rp2040ArduinoMqttTransport::configure(const MqttTransportConfig& config) {
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

void Rp2040ArduinoMqttTransport::setMessageCallback(MqttMessageCallback callback, void* context) {
  _callback = callback;
  _context = context;
}

bool Rp2040ArduinoMqttTransport::connect(const MqttConnectionOptions& options) {
  return _mqtt.connect(options.clientId, options.username, options.password, options.willTopic, 0,
                        options.willRetain, options.willPayload);
}

void Rp2040ArduinoMqttTransport::disconnect() { _mqtt.disconnect(); }

bool Rp2040ArduinoMqttTransport::connected() const { return _mqtt.connected(); }

bool Rp2040ArduinoMqttTransport::subscribe(const char* topic) { return _mqtt.subscribe(topic); }

bool Rp2040ArduinoMqttTransport::publish(const char* topic, const uint8_t* payload, size_t length, bool retained) {
  return _mqtt.publish(topic, payload, length, retained);
}

void Rp2040ArduinoMqttTransport::poll() { _mqtt.loop(); }

int Rp2040ArduinoMqttTransport::lastState() const { return _mqtt.state(); }

void Rp2040ArduinoMqttTransport::pubSubThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (!_activeInstance || !_activeInstance->_callback) return;
  _activeInstance->_callback(_activeInstance->_context, topic, payload, length);
}

#endif  // ARDUINO_ARCH_RP2040
