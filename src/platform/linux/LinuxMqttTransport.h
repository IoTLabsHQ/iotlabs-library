#pragma once

#if !defined(ARDUINO)

#include <mosquitto.h>

#include <string>

#include "../../core/ports/IMqttTransport.h"

// libmosquitto-backed IMqttTransport (confirmed available via
// `pkg-config --exists libmosquitto`, headers at /opt/homebrew/include on
// this dev machine; on real Linux/RPi it's the libmosquitto-dev apt
// package). mosquitto_new(id, clean_session, userdata) is given `this` as
// userdata, so -- unlike the ESP32 adapter's PubSubClient, which only takes
// a bare function pointer and needs a static "active instance" hack --
// every callback below is a real per-instance thunk registered via
// mosquitto_*_callback_set, reading its target straight back out of the
// `obj` parameter mosquitto's C API hands it.
class LinuxMqttTransport : public IMqttTransport {
 public:
  LinuxMqttTransport();
  ~LinuxMqttTransport() override;

  void configure(const MqttTransportConfig& config) override;
  void setMessageCallback(MqttMessageCallback callback, void* context) override;

  // NOTE: connect() calls mosquitto_connect(), which is a BLOCKING TCP+TLS
  // handshake. This is an accepted, deliberate design point for this
  // adapter (see LinuxMqttTransport.cpp): Linux is not a single-
  // cooperative-loop MCU like the ESP32 target this port was originally
  // shaped for, so a blocking connect here does not stall anything else on
  // the host the way it would on a bare-metal cooperative scheduler.
  bool connect(const MqttConnectionOptions& options) override;
  void disconnect() override;
  bool connected() const override;

  bool subscribe(const char* topic) override;
  bool publish(const char* topic, const uint8_t* payload, size_t length, bool retained) override;

  // mosquitto_loop(mosq, /*timeout_ms=*/0, /*max_packets=*/1): a genuinely
  // non-blocking pump, matching the engine's cooperative tick model.
  void poll() override;

  // Most recent CONNACK/disconnect reason code observed via the connect/
  // disconnect callbacks below. Its exact numeric meaning differs from
  // PubSubClient::state() on the ESP32 adapter -- that's expected and
  // accepted: the port only requires connected()/publish()/subscribe() to
  // behave correctly, not identical diagnostic-value semantics across
  // platforms (see IMqttTransport::lastState()'s doc comment).
  int lastState() const override;

 private:
  struct mosquitto* _mosq = nullptr;

  MqttTransportConfig _config;
  MqttMessageCallback _callback = nullptr;
  void* _callbackContext = nullptr;

  bool _connected = false;
  int _lastState = 0;

  // Temp file backing mosquitto_tls_set()'s file-path-only `cafile`
  // parameter (libmosquitto has no in-memory blob API, unlike libcurl's
  // CURLOPT_CAINFO_BLOB used by LinuxHttpClient) -- written once per
  // configure() call, kept alive for the transport's lifetime since a
  // reconnect may re-read it, removed in the destructor.
  std::string _tlsCaTempPath;

  void destroyMosquittoInstance();
  void applyTlsSettings();
  void removeTlsCaTempFile();

  static void onConnect(struct mosquitto* mosq, void* obj, int rc);
  static void onDisconnect(struct mosquitto* mosq, void* obj, int rc);
  static void onMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message);
};

#endif  // !ARDUINO
