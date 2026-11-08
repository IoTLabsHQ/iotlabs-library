#include "LinuxMqttTransport.h"

#if !defined(ARDUINO)

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
// mosquitto_lib_init()/mosquitto_lib_cleanup() must each run exactly once
// per process -- ref-counted here (mirrors LinuxHttpClient's curl_global_
// init/cleanup ref-counting) since more than one LinuxMqttTransport could
// theoretically be constructed.
int g_mosquittoRefCount = 0;
}  // namespace

LinuxMqttTransport::LinuxMqttTransport() {
  if (g_mosquittoRefCount == 0) mosquitto_lib_init();
  ++g_mosquittoRefCount;
}

LinuxMqttTransport::~LinuxMqttTransport() {
  destroyMosquittoInstance();
  removeTlsCaTempFile();
  --g_mosquittoRefCount;
  if (g_mosquittoRefCount == 0) mosquitto_lib_cleanup();
}

void LinuxMqttTransport::destroyMosquittoInstance() {
  if (_mosq) {
    mosquitto_disconnect(_mosq);
    mosquitto_destroy(_mosq);
    _mosq = nullptr;
  }
  _connected = false;
}

void LinuxMqttTransport::configure(const MqttTransportConfig& config) {
  _config = config;
  // bufferBytes has no libmosquitto equivalent -- unlike PubSubClient,
  // libmosquitto manages its own internal buffers dynamically. Accepted
  // (kept for port-contract parity across platforms) but intentionally
  // unused here, not silently dropped without explanation.
}

void LinuxMqttTransport::setMessageCallback(MqttMessageCallback callback, void* context) {
  _callback = callback;
  _callbackContext = context;
}

void LinuxMqttTransport::removeTlsCaTempFile() {
  if (!_tlsCaTempPath.empty()) {
    ::unlink(_tlsCaTempPath.c_str());
    _tlsCaTempPath.clear();
  }
}

void LinuxMqttTransport::applyTlsSettings() {
  if (!_mosq) return;
  removeTlsCaTempFile();

  const bool hasCa = _config.rootCA && _config.rootCA[0] != '\0';
  if (hasCa) {
    // libmosquitto's TLS API is file-path-based only (no in-memory blob
    // support, unlike libcurl's CURLOPT_CAINFO_BLOB used by
    // LinuxHttpClient) -- write the PEM content to a securely-created,
    // 0600 temp file. Kept alive for this transport's lifetime (a
    // reconnect may re-read the same path); removed in the destructor or
    // the next configure()/applyTlsSettings() call.
    const char* tmpdir = std::getenv("TMPDIR");
    if (!tmpdir || tmpdir[0] == '\0') tmpdir = "/tmp";
    std::string tmpl = std::string(tmpdir) + "/iotlabs_mqtt_ca_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    const int fd = ::mkstemp(buf.data());
    if (fd >= 0) {
      ::fchmod(fd, 0600);
      const size_t len = std::strlen(_config.rootCA);
      const bool ok = ::write(fd, _config.rootCA, len) == static_cast<ssize_t>(len);
      ::close(fd);
      if (ok) _tlsCaTempPath = buf.data();
    }
  }

  if (!_tlsCaTempPath.empty()) {
    mosquitto_tls_set(_mosq, _tlsCaTempPath.c_str(), nullptr, nullptr, nullptr, nullptr);
    if (_config.tlsInsecure) mosquitto_tls_insecure_set(_mosq, true);
  } else if (_config.tlsInsecure) {
    // KNOWN LIMITATION (also documented in BOARD_QUALIFICATION.md):
    // mosquitto_tls_set() requires a non-null cafile or capath -- unlike
    // WiFiClientSecure::setInsecure(), libmosquitto has no "skip
    // verification with no CA at all" mode. tlsInsecure=true without a
    // rootCA therefore cannot enable TLS on this adapter; connect() below
    // proceeds in plaintext and will fail against a TLS-only broker (e.g.
    // the default port 8883). Pass tlsInsecure=true together with a rootCA
    // PEM (even a throwaway/self-signed one) to get the intended "skip
    // hostname/chain validation" behavior.
    std::fprintf(stderr,
                  "[LinuxMqttTransport] tlsInsecure=true with no rootCA: libmosquitto requires a CA "
                  "file/path to enable TLS at all. Skipping TLS setup; connect() will attempt "
                  "plaintext and will fail against a TLS-only broker.\n");
  } else {
    std::fprintf(stderr,
                  "[LinuxMqttTransport] no rootCA provided and tlsInsecure=false: TLS will not be "
                  "enabled; connect() will likely fail against a TLS broker.\n");
  }
}

bool LinuxMqttTransport::connect(const MqttConnectionOptions& options) {
  destroyMosquittoInstance();

  _mosq = mosquitto_new(options.clientId, /*clean_session=*/true, this);
  if (!_mosq) {
    _lastState = -1;
    return false;
  }

  mosquitto_connect_callback_set(_mosq, &LinuxMqttTransport::onConnect);
  mosquitto_disconnect_callback_set(_mosq, &LinuxMqttTransport::onDisconnect);
  mosquitto_message_callback_set(_mosq, &LinuxMqttTransport::onMessage);

  applyTlsSettings();

  if (options.username && options.username[0] != '\0') {
    mosquitto_username_pw_set(_mosq, options.username, options.password);
  }

  // Will must be set before connect() per the port contract.
  if (options.willTopic && options.willTopic[0] != '\0') {
    const int willLen = options.willPayload ? static_cast<int>(std::strlen(options.willPayload)) : 0;
    // QoS 1 for the LWT: a Linux daemon can afford the extra broker
    // round-trip guarantee; the port doesn't mandate matching the ESP32
    // adapter's implicit QoS 0 (PubSubClient::connect()'s will overload has
    // no QoS parameter at all).
    mosquitto_will_set(_mosq, options.willTopic, willLen, options.willPayload, /*qos=*/1, options.willRetain);
  }

  _connected = false;
  const int rc = mosquitto_connect(_mosq, _config.host ? _config.host : "", _config.port, _config.keepAliveSeconds);
  _lastState = rc;
  if (rc != MOSQ_ERR_SUCCESS) return false;

  // mosquitto_connect() only performs the TCP(+TLS) handshake and sends the
  // CONNECT packet -- it does NOT wait for the broker's CONNACK. The
  // engine (RuntimeManager::connect() in src/core/RuntimeManager.cpp)
  // expects a `true` return here to mean "actually connected, safe to
  // subscribe/publish immediately", matching the ESP32 adapter's
  // PubSubClient::connect() (which blocks for CONNACK internally). So this
  // pumps mosquitto_loop() itself, bounded by socketTimeoutSeconds, until
  // the onConnect callback below fires. This makes connect() a genuinely
  // blocking call end to end -- an accepted, documented design point for
  // this adapter: Linux is not a single-cooperative-loop MCU the way the
  // ESP32 target this port was originally shaped for, so blocking here for
  // one bounded handshake does not starve anything else on the host.
  const int timeoutSeconds = _config.socketTimeoutSeconds > 0 ? _config.socketTimeoutSeconds : 10;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
  while (!_connected && std::chrono::steady_clock::now() < deadline) {
    mosquitto_loop(_mosq, /*timeout_ms=*/100, /*max_packets=*/1);
  }

  return _connected;
}

void LinuxMqttTransport::disconnect() {
  if (_mosq) mosquitto_disconnect(_mosq);
  _connected = false;
}

bool LinuxMqttTransport::connected() const { return _connected; }

bool LinuxMqttTransport::subscribe(const char* topic) {
  if (!_mosq || !topic) return false;
  // QoS 1: see the will-set comment above for the same reasoning.
  return mosquitto_subscribe(_mosq, nullptr, topic, /*qos=*/1) == MOSQ_ERR_SUCCESS;
}

bool LinuxMqttTransport::publish(const char* topic, const uint8_t* payload, size_t length, bool retained) {
  if (!_mosq || !topic) return false;
  return mosquitto_publish(_mosq, nullptr, topic, static_cast<int>(length), payload, /*qos=*/1, retained) ==
         MOSQ_ERR_SUCCESS;
}

void LinuxMqttTransport::poll() {
  if (!_mosq) return;
  mosquitto_loop(_mosq, /*timeout_ms=*/0, /*max_packets=*/1);
}

int LinuxMqttTransport::lastState() const { return _lastState; }

void LinuxMqttTransport::onConnect(struct mosquitto* mosq, void* obj, int rc) {
  (void)mosq;
  auto* self = static_cast<LinuxMqttTransport*>(obj);
  if (!self) return;
  self->_lastState = rc;
  self->_connected = (rc == 0);  // 0 == CONNACK "accepted" per the MQTT spec / libmosquitto convention.
}

void LinuxMqttTransport::onDisconnect(struct mosquitto* mosq, void* obj, int rc) {
  (void)mosq;
  auto* self = static_cast<LinuxMqttTransport*>(obj);
  if (!self) return;
  self->_lastState = rc;
  self->_connected = false;
}

void LinuxMqttTransport::onMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message) {
  (void)mosq;
  auto* self = static_cast<LinuxMqttTransport*>(obj);
  if (!self || !self->_callback || !message || !message->topic) return;
  self->_callback(self->_callbackContext, message->topic, static_cast<const uint8_t*>(message->payload),
                   static_cast<size_t>(message->payloadlen));
}

#endif  // !ARDUINO
