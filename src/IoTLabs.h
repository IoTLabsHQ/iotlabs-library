#pragma once

#if !defined(ARDUINO_ARCH_ESP32)
#error "IoTLabs currently supports ESP32-family boards only."
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>

#include "internal/IoTLabsTypes.h"
#include "internal/IoTLabsStorage.h"

class IoTLabsValue {
 public:
  explicit IoTLabsValue(JsonVariantConst value) : _value(value) {}

  bool isNull() const { return _value.isNull(); }
  bool isBool() const { return _value.is<bool>(); }
  bool isNumber() const {
    return _value.is<int>() || _value.is<long>() || _value.is<float>() || _value.is<double>();
  }
  bool isString() const { return _value.is<const char*>(); }
  bool isObject() const { return _value.is<JsonObjectConst>(); }
  bool isArray() const { return _value.is<JsonArrayConst>(); }

  bool asBool(bool fallback = false) const;
  long asInt(long fallback = 0) const;
  float asFloat(float fallback = 0.0f) const;
  String asString(const String& fallback = String()) const;

  bool has(const char* key) const;
  IoTLabsValue operator[](const char* key) const;

  JsonVariantConst raw() const { return _value; }

 private:
  JsonVariantConst _value;
};

class IoTLabs {
 public:
  static constexpr uint8_t MAX_CONTROLS = 24;

  using BoolHandler = void (*)(bool value);
  using NumberHandler = void (*)(float value);
  using StringHandler = void (*)(const String& value);
  using JsonHandler = void (*)(const IoTLabsValue& value);
  using EventHandler = void (*)(IoTLabsEvent event);

  IoTLabs();

  // Build-metadata convenience API. Define the IOTLABS_* macros before including IoTLabs.h.
  inline bool begin() {
    IoTLabsConfig config;
#ifdef IOTLABS_PROFILE_ID
    config.profileId = IOTLABS_PROFILE_ID;
#endif
#ifdef IOTLABS_PROFILE_VERSION
    config.profileVersion = IOTLABS_PROFILE_VERSION;
#endif
#ifdef IOTLABS_FIRMWARE_VERSION
    config.firmwareVersion = IOTLABS_FIRMWARE_VERSION;
#endif
#ifdef IOTLABS_DEVICE_PLATFORM_URL
    config.devicePlatformUrl = IOTLABS_DEVICE_PLATFORM_URL;
#endif
#ifdef IOTLABS_TLS_INSECURE
    config.tlsInsecure = IOTLABS_TLS_INSECURE;
#endif
#ifdef IOTLABS_DEVICE_PLATFORM_ROOT_CA
    config.devicePlatformRootCA = IOTLABS_DEVICE_PLATFORM_ROOT_CA;
#endif
#ifdef IOTLABS_MQTT_ROOT_CA
    config.mqttRootCA = IOTLABS_MQTT_ROOT_CA;
#endif
#ifdef IOTLABS_PROVISIONING_AP_PASSWORD
    config.provisioningApPassword = IOTLABS_PROVISIONING_AP_PASSWORD;
#endif
    return begin(config);
  }

  bool begin(const IoTLabsConfig& config);
  void loop();

  bool on(const char* controlId, BoolHandler handler);
  bool on(const char* controlId, NumberHandler handler);
  bool on(const char* controlId, StringHandler handler);
  bool on(const char* controlId, JsonHandler handler);

  bool report(const char* controlId, bool value, bool publishNow = true);
  bool report(const char* controlId, int value, bool publishNow = true);
  bool report(const char* controlId, long value, bool publishNow = true);
  bool report(const char* controlId, float value, bool publishNow = true);
  bool report(const char* controlId, double value, bool publishNow = true);
  bool report(const char* controlId, const char* value, bool publishNow = true);
  bool report(const char* controlId, const String& value, bool publishNow = true);
  bool report(const char* controlId, JsonVariantConst value, bool publishNow = true);

  bool connected() const;
  bool wifiConnected() const;
  bool provisioned() const;
  bool provisioning() const;
  IoTLabsStatus status() const;

  void onEvent(EventHandler handler);

  // Device lifecycle helpers.
  bool startProvisioning();
  bool resetWiFi();
  bool factoryReset(bool restartDevice = true);

  bool publishTelemetrySnapshot();
  bool publishStatus();

  String hardwareId() const { return _hardwareId; }
  String deviceId() const { return _runtime.deviceId; }
  String profileId() const { return String(_config.profileId ? _config.profileId : ""); }
  uint32_t profileVersion() const { return _config.profileVersion; }
  String provisioningApName() const { return _apName; }
  String lastError() const { return _lastError; }

  int mqttState() const;
  uint32_t reconnectDelayMs() const;

 private:
  enum class HandlerKind : uint8_t { NONE, BOOL, NUMBER, STRING, JSON };
  enum class StateKind : uint8_t { NONE, BOOL, INTEGER, NUMBER, STRING, JSON };

  struct HandlerEntry {
    bool used = false;
    String id;
    HandlerKind kind = HandlerKind::NONE;
    BoolHandler boolHandler = nullptr;
    NumberHandler numberHandler = nullptr;
    StringHandler stringHandler = nullptr;
    JsonHandler jsonHandler = nullptr;
  };

  struct StateEntry {
    bool used = false;
    String id;
    StateKind kind = StateKind::NONE;
    bool boolValue = false;
    long intValue = 0;
    double numberValue = 0.0;
    String stringValue;
    String jsonValue;
  };

  IoTLabsConfig _config;
  IoTLabsStorage _storage;
  IoTLabsRuntimeIdentity _runtime;

  WiFiClientSecure _mqttSecureClient;
  PubSubClient _mqtt;
  WebServer _provisionServer;

  HandlerEntry _handlers[MAX_CONTROLS];
  StateEntry _states[MAX_CONTROLS];

  EventHandler _eventHandler = nullptr;
  IoTLabsStatus _status = IoTLabsStatus::BOOT;

  bool _started = false;
  bool _provisioningActive = false;
  bool _seenWifiConnected = false;
  bool _seenMqttConnected = false;
  bool _wifiAttemptActive = false;
  bool _activationBlocked = false;
  bool _mqttConfigured = false;
  bool _provisionRoutesConfigured = false;

  String _hardwareId;
  String _apName;
  String _lastError;
  String _wifiSsid;
  String _wifiPassword;
  String _activationToken;

  String _topicTelemetry;
  String _topicCmd;
  String _topicStatus;
  String _topicPing;

  uint32_t _wifiBackoffMs = 1000;
  uint32_t _mqttBackoffMs = 1000;
  uint32_t _activationBackoffMs = 2000;

  uint32_t _nextWifiAttemptAt = 0;
  uint32_t _wifiAttemptStartedAt = 0;
  uint32_t _nextMqttAttemptAt = 0;
  uint32_t _nextActivationAttemptAt = 0;
  uint32_t _lastTelemetryAt = 0;
  uint32_t _lastStatusAt = 0;

  static IoTLabs* _instance;

  static bool due(uint32_t now, uint32_t deadline);
  static void mqttCallbackThunk(char* topic, byte* payload, unsigned int length);

  void emit(IoTLabsEvent event);
  void setStatus(IoTLabsStatus status);
  void setError(const String& code);

  bool validateConfig();
  void loadPersistentState();
  void generateHardwareId();

  bool startProvisioningPortal();
  void stopProvisioningPortal();
  void configureProvisioningRoutes();
  void handleDeviceInfoRequest();
  void handleWifiNetworksRequest();
  void handleProvisionRequest();
  void sendLocalError(int httpStatus, const char* code, const char* message);

  void serviceWiFi(uint32_t now);
  void startWifiAttempt(uint32_t now);
  void serviceActivation(uint32_t now);
  bool attemptActivation();
  bool activationErrorIsBlocking(const String& code) const;
  String activationUrl() const;

  bool configureMqtt();
  void buildTopics();
  void serviceMqtt(uint32_t now);
  bool connectMqtt();
  void onMqttConnected();

  void handleMessage(char* topic, byte* payload, unsigned int length);
  void handleCommand(const String& message);
  void handlePing(const String& message);

  HandlerEntry* findHandler(const String& controlId);
  HandlerEntry* findOrCreateHandler(const String& controlId);
  StateEntry* findState(const String& controlId);
  StateEntry* findOrCreateState(const String& controlId);

  bool reportState(StateEntry& state, bool publishNow);
  bool publishSingleState(const StateEntry& state);
  void appendState(JsonDocument& doc, const StateEntry& state) const;
  bool publishJson(const String& topic, JsonDocument& doc, bool retained = false);

  static bool parseBool(JsonVariantConst value, bool& result);
};
