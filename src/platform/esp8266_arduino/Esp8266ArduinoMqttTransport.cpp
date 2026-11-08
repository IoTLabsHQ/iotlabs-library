#include "Esp8266ArduinoMqttTransport.h"

// Esp8266ArduinoMqttTransport.h itself no-ops outside ESP8266 builds (it
// includes <WiFiClientSecureBearSSL.h>), so guard this .cpp the same way --
// otherwise every member definition below would be an error against an
// incomplete/undeclared class on another board.
#if defined(ARDUINO_ARCH_ESP8266)

Esp8266ArduinoMqttTransport* Esp8266ArduinoMqttTransport::_activeInstance = nullptr;

Esp8266ArduinoMqttTransport::Esp8266ArduinoMqttTransport() : _mqtt(_secureClient) {
  _activeInstance = this;
  _mqtt.setCallback(&Esp8266ArduinoMqttTransport::pubSubThunk);
}

void Esp8266ArduinoMqttTransport::configure(const MqttTransportConfig& config) {
  if (config.tlsInsecure || !config.rootCA) {
    _secureClient.setInsecure();
    _trustAnchor.reset();
  } else {
    _trustAnchor.reset(new BearSSL::X509List(config.rootCA));
    _secureClient.setTrustAnchors(_trustAnchor.get());
  }

  _mqtt.setServer(config.host, config.port);
  _mqtt.setKeepAlive(config.keepAliveSeconds);
  _mqtt.setSocketTimeout(config.socketTimeoutSeconds);
  _mqtt.setBufferSize(config.bufferBytes);
}

void Esp8266ArduinoMqttTransport::setMessageCallback(MqttMessageCallback callback, void* context) {
  _callback = callback;
  _context = context;
}

bool Esp8266ArduinoMqttTransport::connect(const MqttConnectionOptions& options) {
  return _mqtt.connect(options.clientId, options.username, options.password, options.willTopic, 0,
                        options.willRetain, options.willPayload);
}

void Esp8266ArduinoMqttTransport::disconnect() { _mqtt.disconnect(); }

bool Esp8266ArduinoMqttTransport::connected() const { return _mqtt.connected(); }

bool Esp8266ArduinoMqttTransport::subscribe(const char* topic) { return _mqtt.subscribe(topic); }

bool Esp8266ArduinoMqttTransport::publish(const char* topic, const uint8_t* payload, size_t length, bool retained) {
  return _mqtt.publish(topic, payload, length, retained);
}

void Esp8266ArduinoMqttTransport::poll() { _mqtt.loop(); }

int Esp8266ArduinoMqttTransport::lastState() const { return _mqtt.state(); }

void Esp8266ArduinoMqttTransport::pubSubThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (!_activeInstance || !_activeInstance->_callback) return;
  _activeInstance->_callback(_activeInstance->_context, topic, payload, length);
}

#endif  // ARDUINO_ARCH_ESP8266
