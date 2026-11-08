#include "RuntimeManager.h"

#include <ArduinoJson.h>
#include <cstring>

namespace {
bool due(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
// Reserved controlId that never corresponds to a real Control — Device
// Platform's automation-sync push (see AutomationsService on the backend)
// addresses it instead, reusing the existing /cmd envelope/topic rather than
// needing a new MQTT subscription. See AutomationEngine.h for the payload
// shape carried in "value".
const char* AUTOMATION_SYNC_TARGET = "__automation_rules__";
}  // namespace

RuntimeManager::RuntimeManager(IMqttTransport& mqtt, IClock& clock, ControlRouter& controls, StateRegistry& states,
                                AutomationEngine& automation, ILogger* logger)
    : _mqtt(mqtt), _clock(clock), _controls(controls), _states(states), _automation(automation), _logger(logger) {
  _mqtt.setMessageCallback(&RuntimeManager::messageThunk, this);
}

void RuntimeManager::configureStatic(const StaticConfig& config, const std::string& hardwareId) {
  _config = config;
  _hardwareId = hardwareId;
  resetBackoff();
}

void RuntimeManager::setEventCallback(EventCallback callback, void* context) {
  _eventCallback = callback;
  _eventContext = context;
}

void RuntimeManager::setCommandFinishCallback(CommandFinishCallback callback, void* context) {
  _commandFinishCallback = callback;
  _commandFinishContext = context;
}

void RuntimeManager::emit(IoTLabsEvent event) {
  if (_eventCallback) _eventCallback(_eventContext, event);
}

void RuntimeManager::emitCommandFinish(const char* controlId) {
  if (_commandFinishCallback) _commandFinishCallback(_commandFinishContext, controlId);
}

void RuntimeManager::resetBackoff() {
  _backoffMs = _config.reconnectMinMs > 1000 ? _config.reconnectMinMs : 1000;
}

bool RuntimeManager::configureIdentity(const IoTLabsRuntimeIdentity& identity, const char* rootCA) {
  if (!identity.valid()) return false;
  if (!_config.tlsInsecure && (!rootCA || rootCA[0] == '\0')) return false;

  _identity = identity;
  _topics.build(_identity.topicPrefix, _identity.mqttClientId);

  MqttTransportConfig transportConfig;
  transportConfig.host = _identity.mqttBroker.c_str();
  transportConfig.port = _identity.mqttPort;
  transportConfig.tlsInsecure = _config.tlsInsecure;
  transportConfig.rootCA = rootCA;
  transportConfig.keepAliveSeconds = _config.keepAliveSeconds;
  transportConfig.socketTimeoutSeconds = _config.socketTimeoutSeconds;
  transportConfig.bufferBytes = _config.mqttBufferBytes;
  _mqtt.configure(transportConfig);

  _configured = true;
  return true;
}

bool RuntimeManager::connected() const { return _mqtt.connected(); }

void RuntimeManager::forceDisconnect() {
  if (_mqtt.connected()) _mqtt.disconnect();
  _seenConnected = false;
}

bool RuntimeManager::connect() {
  iotlabs_log::line(_logger, "[IoTLabs] Cloud connecting...");

  JsonDocument will;
  will["status"] = "offline";
  will["device_id"] = _identity.deviceId;
  will["device_template_id"] = _config.deviceTemplateId;
  will["sdk_version"] = IOTLABS_SDK_VERSION;
  std::string willPayload;
  serializeJson(will, willPayload);

  MqttConnectionOptions options;
  options.clientId = _identity.mqttClientId.c_str();
  options.username = _identity.mqttUsername.c_str();
  options.password = _identity.mqttPassword.c_str();
  options.willTopic = _topics.status().c_str();
  options.willPayload = willPayload.c_str();
  options.willRetain = true;

  const bool ok = _mqtt.connect(options);
  if (!ok) {
    iotlabs_log::line(_logger, "[IoTLabs] Cloud connect failed rc=%d", _mqtt.lastState());
    return false;
  }

  iotlabs_log::line(_logger, "[IoTLabs] Cloud connect OK");
  return true;
}

void RuntimeManager::onConnected(uint32_t now) {
  _mqtt.subscribe(_topics.cmd().c_str());
  _mqtt.subscribe(_topics.ping().c_str());

  publishStatus();
  publishTelemetrySnapshot();

  _lastTelemetryAt = now;
  _lastStatusAt = now;

  iotlabs_log::line(_logger, "[IoTLabs] ONLINE");
  emit(IoTLabsEvent::ONLINE);
}

void RuntimeManager::tick(uint32_t now) {
  if (!_configured) return;

  if (_mqtt.connected()) {
    if (!_seenConnected) {
      _seenConnected = true;
      onConnected(now);
    }
  } else {
    if (_seenConnected) {
      _seenConnected = false;
      iotlabs_log::line(_logger, "[IoTLabs] Cloud disconnected");
      emit(IoTLabsEvent::OFFLINE);
    }

    if (due(now, _nextAttemptAt)) {
      emit(IoTLabsEvent::CLOUD_CONNECTING);
      if (connect()) {
        resetBackoff();
        _nextAttemptAt = now;
        _seenConnected = true;
        onConnected(now);
      } else {
        _nextAttemptAt = now + _backoffMs;
        _backoffMs = (_backoffMs * 2) < _config.reconnectMaxMs ? (_backoffMs * 2) : _config.reconnectMaxMs;
      }
    }
    return;
  }

  _mqtt.poll();

  if (_config.telemetryIntervalMs > 0 && due(now, _lastTelemetryAt + _config.telemetryIntervalMs)) {
    _lastTelemetryAt = now;
    publishTelemetrySnapshot();
  }
  if (_config.statusIntervalMs > 0 && due(now, _lastStatusAt + _config.statusIntervalMs)) {
    _lastStatusAt = now;
    publishStatus();
  }
}

bool RuntimeManager::publishJson(const std::string& topic, const std::string& payload, bool retained) {
  if (!_mqtt.connected()) return false;
  const bool ok = _mqtt.publish(topic.c_str(), reinterpret_cast<const uint8_t*>(payload.data()), payload.size(),
                                 retained);
  if (ok) emit(IoTLabsEvent::PUBLISHED);
  return ok;
}

bool RuntimeManager::publishSingleState(const char* controlId) {
  if (!_mqtt.connected()) return false;
  JsonDocument doc;
  if (!_states.appendOne(doc, controlId)) return true;
  doc["timestamp"] = _clock.monotonicMillis();
  doc["uptime_ms"] = _clock.monotonicMillis();
  std::string payload;
  serializeJson(doc, payload);
  return publishJson(_topics.telemetry(), payload, false);
}

bool RuntimeManager::publishTelemetrySnapshot() {
  if (!_mqtt.connected()) return false;
  JsonDocument doc;
  if (!_states.appendAll(doc)) return true;
  doc["timestamp"] = _clock.monotonicMillis();
  doc["uptime_ms"] = _clock.monotonicMillis();
  std::string payload;
  serializeJson(doc, payload);
  return publishJson(_topics.telemetry(), payload, false);
}

bool RuntimeManager::publishStatus() {
  if (!_mqtt.connected()) return false;
  JsonDocument doc;
  doc["status"] = "online";
  doc["device_id"] = _identity.deviceId;
  doc["hardware_id"] = _hardwareId;
  doc["device_template_id"] = _config.deviceTemplateId;
  doc["device_template_version"] = _config.deviceTemplateVersion;
  doc["firmware_version"] = _config.firmwareVersion;
  doc["sdk_version"] = IOTLABS_SDK_VERSION;
  doc["protocol_version"] = IOTLABS_PROTOCOL_VERSION;
  doc["provisioning_version"] = IOTLABS_PROVISIONING_VERSION;

  _states.appendAll(doc);

  doc["timestamp"] = _clock.monotonicMillis();
  doc["uptime_ms"] = _clock.monotonicMillis();
  std::string payload;
  serializeJson(doc, payload);
  return publishJson(_topics.status(), payload, true);
}

void RuntimeManager::handlePing(const std::string& message) {
  JsonDocument incoming;
  if (!deserializeJson(incoming, message)) {
    const char* type = incoming["type"] | "";
    if (std::string(type) == "pong") return;  // Prevent self-echo loops on brokers that echo own publish.
  }

  JsonDocument pong;
  pong["type"] = "pong";
  pong["device_id"] = _identity.deviceId;
  pong["hardware_id"] = _hardwareId;
  pong["uptime_ms"] = _clock.monotonicMillis();
  std::string payload;
  serializeJson(pong, payload);
  publishJson(_topics.ping(), payload, false);
}

void RuntimeManager::handleCommand(const std::string& message) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, message);
  if (error) {
    emit(IoTLabsEvent::MESSAGE_ERROR);
    return;
  }

  const char* target = doc["target"] | "";
  const char* command = doc["command"] | "";
  if (target[0] == '\0' || command[0] == '\0') {
    emit(IoTLabsEvent::MESSAGE_ERROR);
    return;
  }

  // Automation-rule sync — not a real Control, doesn't go through
  // ControlRouter/publishStatus (see AUTOMATION_SYNC_TARGET above).
  if (strcmp(target, AUTOMATION_SYNC_TARGET) == 0 && strcmp(command, "sync") == 0) {
    _automation.syncFromJson(doc["value"]);
    return;
  }

  bool handlerFound = false;
  const bool ok = _controls.dispatch(target, doc["value"], handlerFound);
  if (!handlerFound || !ok) {
    emit(IoTLabsEvent::MESSAGE_ERROR);
    return;
  }

  // Fire the command-finish hook BEFORE the unconditional publishStatus()
  // below. This ordering matters: an app that registers onCommandFinish()
  // can synchronously call report()/reportBool() from inside that callback
  // for a fast/sync action, and that value will already be in StateRegistry
  // in time to be included in the publishStatus() snapshot that immediately
  // follows. Apps that don't register the hook see zero behavior change —
  // _commandFinishCallback stays null and this is a no-op.
  emitCommandFinish(target);

  // Status includes only actual states reported by the application.
  publishStatus();
}

void RuntimeManager::handleMessage(const char* topic, const uint8_t* payload, size_t length) {
  std::string message(reinterpret_cast<const char*>(payload), length);

  const std::string topicString(topic);
  if (topicString == _topics.ping()) {
    handlePing(message);
    return;
  }
  if (topicString == _topics.cmd()) handleCommand(message);
}

void RuntimeManager::messageThunk(void* context, const char* topic, const uint8_t* payload, size_t length) {
  static_cast<RuntimeManager*>(context)->handleMessage(topic, payload, length);
}
