#pragma once

#include <cstdint>
#include <string>

#include "../IoTLabsTypes.h"
#include "AutomationEngine.h"
#include "ControlRouter.h"
#include "IoTLabsRuntimeIdentity.h"
#include "Log.h"
#include "StateRegistry.h"
#include "TopicResolver.h"
#include "ports/IClock.h"
#include "ports/IMqttTransport.h"
#include "ports/IStorage.h"

// MQTT Runtime Protocol v1 connect/publish/dispatch (IoTLabs internal design notes),
// ported 1:1 from the original serviceMqtt()/connectMqtt()/handleMessage().
class RuntimeManager {
 public:
  struct StaticConfig {
    const char* deviceTemplateId = nullptr;
    uint32_t deviceTemplateVersion = 1;
    const char* firmwareVersion = nullptr;
    bool tlsInsecure = false;
    uint32_t reconnectMinMs = 1000;
    uint32_t reconnectMaxMs = 30000;
    uint32_t telemetryIntervalMs = 10000;
    uint32_t statusIntervalMs = 30000;
    uint16_t keepAliveSeconds = 60;
    uint16_t socketTimeoutSeconds = 10;
    uint16_t mqttBufferBytes = 1536;
  };

  using EventCallback = void (*)(void* context, IoTLabsEvent event);
  using CommandFinishCallback = void (*)(void* context, const char* controlId);

  RuntimeManager(IMqttTransport& mqtt, IClock& clock, ControlRouter& controls, StateRegistry& states,
                 AutomationEngine& automation, ILogger* logger);

  void configureStatic(const StaticConfig& config, const std::string& hardwareId);
  void setEventCallback(EventCallback callback, void* context);
  void setCommandFinishCallback(CommandFinishCallback callback, void* context);

  // Rebuilds transport config + topics from a freshly activated identity.
  // Returns false if TLS is required but no root CA is available.
  bool configureIdentity(const IoTLabsRuntimeIdentity& identity, const char* rootCA);

  void resetBackoff();
  void requestImmediateAttempt(uint32_t now) { _nextAttemptAt = now; }
  void tick(uint32_t now);
  bool connected() const;
  void forceDisconnect();
  int lastState() const { return _mqtt.lastState(); }
  uint32_t backoffMs() const { return _backoffMs; }

  bool publishTelemetrySnapshot();
  bool publishSingleState(const char* controlId);
  bool publishStatus();

 private:
  IMqttTransport& _mqtt;
  IClock& _clock;
  ControlRouter& _controls;
  StateRegistry& _states;
  AutomationEngine& _automation;
  ILogger* _logger;

  StaticConfig _config;
  std::string _hardwareId;
  IoTLabsRuntimeIdentity _identity;
  TopicResolver _topics;
  bool _configured = false;

  bool _seenConnected = false;
  uint32_t _backoffMs = 1000;
  uint32_t _nextAttemptAt = 0;
  uint32_t _lastTelemetryAt = 0;
  uint32_t _lastStatusAt = 0;

  EventCallback _eventCallback = nullptr;
  void* _eventContext = nullptr;

  CommandFinishCallback _commandFinishCallback = nullptr;
  void* _commandFinishContext = nullptr;

  void emit(IoTLabsEvent event);
  void emitCommandFinish(const char* controlId);
  bool connect();
  void onConnected(uint32_t now);
  bool publishJson(const std::string& topic, const std::string& payload, bool retained);

  void handleMessage(const char* topic, const uint8_t* payload, size_t length);
  void handleCommand(const std::string& message);
  void handlePing(const std::string& message);

  static void messageThunk(void* context, const char* topic, const uint8_t* payload, size_t length);
};
