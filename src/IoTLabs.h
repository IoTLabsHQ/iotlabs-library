#pragma once

// This facade requires an Arduino-compatible core (for Arduino.h's `String`,
// used on the public String-returning getters — see ADR-006 for why that's
// an accepted exception to "no Arduino String" outside core). Non-Arduino
// native builds (Linux, host-side tests) don't include this header at all;
// they drive src/core/IoTLabsEngine.h directly with their own port
// implementations instead — see IoTLabs internal design notes
// for the pattern.
//
// The zero-arg `IoTLabs iot;` default composition (IoTLabs.cpp) is wired
// for ESP32 (CURRENT/qualified) and ESP8266, RP2040, STM32 (CANDIDATE —
// compiles, not yet hardware-qualified; STM32's factory covers exactly one
// board+connectivity combination so far, no TLS — see IoTLabs internal design notes
// and IoTLabs internal design notes
// for what each actually covers). Building for any
// other Arduino-compatible board still compiles this header fine; only the
// zero-arg constructor is
// unavailable (a clear link error if actually called) — use the advanced
// `IoTLabs(IoTLabsDependencies)` constructor with your own platform factory
// instead (IoTLabs internal design notes). This is
// the one platform-detection branch this facade is allowed to own
// ("Preprocessor rule" in that same doc); every other platform-specific
// concern lives behind ports/adapters under src/core and src/platform.
#if !defined(ARDUINO)
#error "src/IoTLabs.h requires an Arduino-compatible core. For native/non-Arduino builds, drive src/core/IoTLabsEngine.h directly with your own port implementations instead of this facade."
#endif

#include <Arduino.h>
#include <ArduinoJson.h>

#include "DefaultRootCA.h"
#include "IoTLabsTypes.h"
#include "IoTLabsValue.h"
#include "core/ControlRouter.h"
#include "core/Dependencies.h"

// Public, platform-neutral facade (IoTLabs internal design notes).
// Notably absent from this header, on purpose: WiFi.h, WiFiClientSecure.h,
// PubSubClient.h, WebServer.h, Preferences.h, esp_system.h. All ESP32/
// third-party networking types stay behind src/platform/esp32_arduino/ and
// src/core/IoTLabsEngine (PIMPL'd via `_impl` below), so including this
// header never pulls platform implementation into application code.
class IoTLabs {
 public:
  static constexpr uint8_t MAX_CONTROLS = ControlRouter::MAX_CONTROLS;

  using BoolHandler = ControlRouter::BoolHandler;
  using NumberHandler = ControlRouter::NumberHandler;
  using StringHandler = ControlRouter::StringHandler;
  using JsonHandler = ControlRouter::JsonHandler;
  using EventHandler = void (*)(IoTLabsEvent event);
  using CommandFinishHandler = void (*)(const char* controlId);

  // Default composition: selects the ESP32 Arduino adapters at build time.
  IoTLabs();

  // Advanced/test composition (IoTLabs internal design notes).
  // Every pointer in `dependencies` must be non-null and must outlive this
  // object; IoTLabs does not take ownership of injected adapters.
  explicit IoTLabs(const IoTLabsDependencies& dependencies);

  ~IoTLabs();
  IoTLabs(const IoTLabs&) = delete;
  IoTLabs& operator=(const IoTLabs&) = delete;

  // Build-metadata convenience API. Define the IOTLABS_* macros before including IoTLabs.h.
  inline bool begin() {
    IoTLabsConfig config;
#ifdef IOTLABS_DEVICE_TEMPLATE_ID
    config.deviceTemplateId = IOTLABS_DEVICE_TEMPLATE_ID;
#endif
#ifdef IOTLABS_DEVICE_TEMPLATE_VERSION
    config.deviceTemplateVersion = IOTLABS_DEVICE_TEMPLATE_VERSION;
#endif
#ifdef IOTLABS_FIRMWARE_VERSION
    config.firmwareVersion = IOTLABS_FIRMWARE_VERSION;
#endif
#ifdef IOTLABS_PROVISIONING_URL
    config.provisioningUrl = IOTLABS_PROVISIONING_URL;
#endif
    // No #else default -- IOTLABS_PROVISIONING_URL is REQUIRED (multi-region
    // deployments: sg./vn./us.iotlabs.vn, or self-hosted). validateConfig()
    // rejects begin() with PROVISIONING_URL_REQUIRED if left undefined.
#ifdef IOTLABS_PROVISIONING_ROOT_CA
    config.provisioningRootCA = IOTLABS_PROVISIONING_ROOT_CA;
#else
    // Default root CA -- every current regional Device Platform host and the
    // production MQTT brokers are Let's Encrypt-issued (verified against the
    // live endpoints -- see DefaultRootCA.h), so this embedded ISRG Root X1
    // validates all of them out of the box regardless of which
    // IOTLABS_PROVISIONING_URL region is configured. tlsInsecure stays at
    // its struct default (false): sketches no longer need to define either
    // this macro or IOTLABS_TLS_INSECURE unless targeting a self-hosted
    // Device Platform with a different CA.
    config.provisioningRootCA = IOTLABS_DEFAULT_ROOT_CA;
#endif
#ifdef IOTLABS_MQTT_ROOT_CA
    config.mqttRootCA = IOTLABS_MQTT_ROOT_CA;
#endif
#ifdef IOTLABS_PROVISIONING_AP_PASSWORD
    config.provisioningApPassword = IOTLABS_PROVISIONING_AP_PASSWORD;
#endif
#ifdef IOTLABS_PROJECT_NAME
    config.provisioningPageTitle = IOTLABS_PROJECT_NAME;
#endif
#ifdef IOTLABS_DEVICE_ID
    config.deviceId = IOTLABS_DEVICE_ID;
#endif
#ifdef IOTLABS_DEVICE_ACTIVATION_SECRET
    config.activationSecret = IOTLABS_DEVICE_ACTIVATION_SECRET;
#endif
#ifdef IOTLABS_WIFI_SSID
    config.wifiSsid = IOTLABS_WIFI_SSID;
#endif
#ifdef IOTLABS_WIFI_PASSWORD
    config.wifiPassword = IOTLABS_WIFI_PASSWORD;
#endif
    return begin(config);
  }

  bool begin(const IoTLabsConfig& config);
  void loop();

  bool on(const char* controlId, BoolHandler handler);
  bool on(const char* controlId, NumberHandler handler);
  bool on(const char* controlId, StringHandler handler);
  bool on(const char* controlId, JsonHandler handler);

  // Optional convenience layer over on()/report(): declares a direct GPIO
  // pin <-> control-id wiring once, and loop() takes care of the rest
  // (debounce+report for inputs, digitalWrite/analogWrite for outputs on
  // command) -- see IoTLabs internal design notes
  // Entirely optional: sketches that never call setPinMapping() are
  // unaffected, and on()/report() keep working exactly as before for any
  // control not covered here (e.g. sensors read through their own driver
  // library, like DHT/I2C -- those have no single-raw-pin semantics).
  //
  // Tag types below select which of the 4 overloads applies (an enum VALUE
  // can't be used as a parameter's static type, so this is the standard
  // tag-dispatch idiom -- same shape as std::nothrow/std::in_place).
  struct DigitalOutTag {};
  struct DigitalInDebouncedTag {};
  struct AnalogInTag {};
  struct PwmOutTag {};
  static constexpr DigitalOutTag DIGITAL_OUT{};
  static constexpr DigitalInDebouncedTag DIGITAL_IN_DEBOUNCED{};
  static constexpr AnalogInTag ANALOG_IN{};
  static constexpr PwmOutTag PWM_OUT{};

  // Digital output (actuator, e.g. relay): iot.setPinMapping(id, pin, IoTLabs::DIGITAL_OUT).
  // Boot-glitch-safe by construction: the pin's off-level is latched via
  // digitalWrite() BEFORE pinMode(OUTPUT) runs.
  bool setPinMapping(const char* controlId, uint8_t pin, DigitalOutTag, bool activeLow = false);

  // Digital input with debounce (button/trigger or state switch). When
  // reportValueOnRelease is set, that fixed string is reported on the
  // active->inactive edge only (matches a momentary-button "pressed" event,
  // e.g. examples/07's boot button); when null, the debounced bool state is
  // reported on every edge instead (state-switch use case).
  bool setPinMapping(const char* controlId, uint8_t pin, DigitalInDebouncedTag, bool activeLow = false,
                      uint32_t debounceMs = 50, const char* reportValueOnRelease = nullptr);

  // Analog input (raw ADC sensor -- not a library-driven sensor like DHT/I2C).
  // Scales [rawMin,rawMax] to [scaledMin,scaledMax] and only reports once
  // pollIntervalMs has elapsed AND the value moved by at least changeThreshold
  // since the last report (the first sample always reports).
  bool setPinMapping(const char* controlId, uint8_t pin, AnalogInTag, float rawMin = 0, float rawMax = 4095,
                      float scaledMin = 0, float scaledMax = 100, uint32_t pollIntervalMs = 200,
                      float changeThreshold = 0);

  // PWM output (dimmer/analog actuator, 0-100 logical duty mapped into
  // [pwmMin,pwmMax] via analogWrite()).
  bool setPinMapping(const char* controlId, uint8_t pin, PwmOutTag, bool activeLow = false,
                      uint16_t pwmMin = 0, uint16_t pwmMax = 255);

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

  // Fires after a write Control's handler has returned successfully, right
  // before the SDK's automatic publishStatus() snapshot. Use it to detect
  // when a command has been dispatched — NOT as a place to report a final
  // value for an asynchronous/non-blocking action (see the
  // QuickStart_ServoSweep example under examples/).
  void onCommandFinish(CommandFinishHandler handler);

  // Device lifecycle helpers.
  bool startProvisioning();
  bool resetWiFi();
  bool resetIdentity();
  bool factoryReset(bool restartDevice = true);

  bool publishTelemetrySnapshot();
  bool publishStatus();

  String hardwareId() const;
  String deviceId() const;
  String deviceTemplateId() const;
  uint32_t deviceTemplateVersion() const;
  String provisioningApName() const;
  String lastError() const;

  int mqttState() const;
  uint32_t reconnectDelayMs() const;

 private:
  struct Impl;
  Impl* _impl;
};
