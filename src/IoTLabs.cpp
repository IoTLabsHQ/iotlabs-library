#include "IoTLabs.h"

#include "PinMappingRegistry.h"
#include "core/IoTLabsEngine.h"

// Composition-root platform selection (the one place this facade is allowed
// a platform #if ladder — see the comment in IoTLabs.h). Exactly one branch
// is compiled in, so only that one platform's headers (WiFi/PubSubClient/
// WebServer/Preferences-equivalents) ever reach this translation unit.
//
// Building for an Arduino-compatible core outside this ladder still
// compiles IoTLabs.h/.cpp fine (Impl/PIMPL machinery below doesn't need a
// platform type) — it just has no `DefaultPlatform`, so the zero-arg
// `IoTLabs()` constructor is deliberately left undefined past this point,
// producing a clear link error only if something actually calls it. Use
// the advanced `IoTLabs(IoTLabsDependencies)` constructor there instead.
#if defined(ARDUINO_ARCH_ESP32)
#include "platform/esp32_arduino/Esp32ArduinoPlatform.h"
using DefaultPlatform = Esp32ArduinoPlatform;
#define IOTLABS_HAS_DEFAULT_PLATFORM 1
#elif defined(ARDUINO_ARCH_ESP8266)
#include "platform/esp8266_arduino/Esp8266ArduinoPlatform.h"
using DefaultPlatform = Esp8266ArduinoPlatform;
#define IOTLABS_HAS_DEFAULT_PLATFORM 1
#elif defined(ARDUINO_ARCH_RP2040)
#include "platform/rp2040_arduino/Rp2040ArduinoPlatform.h"
using DefaultPlatform = Rp2040ArduinoPlatform;
#define IOTLABS_HAS_DEFAULT_PLATFORM 1
#elif defined(ARDUINO_ARCH_STM32)
// CANDIDATE, qualified so far only against a Nucleo-144 NUCLEO_F429ZI board
// (on-board Ethernet, no TLS — see IoTLabs internal design notes).
// STM32 covers many boards/connectivity paths this one factory does not;
// a board without this exact adapter set still needs the advanced
// IoTLabs(IoTLabsDependencies) constructor with its own platform factory.
#include "platform/stm32_arduino/Stm32ArduinoPlatform.h"
using DefaultPlatform = Stm32ArduinoPlatform;
#define IOTLABS_HAS_DEFAULT_PLATFORM 1
#endif

// PIMPL body: only this translation unit needs to see IoTLabsEngine (which
// pulls in the core/ports) or any platform factory (which pulls in that
// platform's networking/storage headers). IoTLabs.h stays clean.
struct IoTLabs::Impl {
#if defined(IOTLABS_HAS_DEFAULT_PLATFORM)
  DefaultPlatform* platform = nullptr;  // owned only by the default (zero-arg) constructor
#endif
  IoTLabsEngine engine;

  explicit Impl(const IoTLabsDependencies& deps) : engine(deps) {}
  ~Impl() {
#if defined(IOTLABS_HAS_DEFAULT_PLATFORM)
    delete platform;
#endif
  }
};

#if defined(IOTLABS_HAS_DEFAULT_PLATFORM)
IoTLabs::IoTLabs() {
  DefaultPlatform* platform = new DefaultPlatform();
  _impl = new Impl(platform->dependencies());
  _impl->platform = platform;
}
#endif

IoTLabs::IoTLabs(const IoTLabsDependencies& dependencies) : _impl(new Impl(dependencies)) {}

IoTLabs::~IoTLabs() { delete _impl; }

bool IoTLabs::begin(const IoTLabsConfig& config) { return _impl->engine.begin(config); }
void IoTLabs::loop() {
  _impl->engine.loop();
  PinMappingRegistry::instance().loop(_impl->engine);
}

bool IoTLabs::on(const char* controlId, BoolHandler handler) { return _impl->engine.onBool(controlId, handler); }
bool IoTLabs::on(const char* controlId, NumberHandler handler) { return _impl->engine.onNumber(controlId, handler); }
bool IoTLabs::on(const char* controlId, StringHandler handler) { return _impl->engine.onString(controlId, handler); }
bool IoTLabs::on(const char* controlId, JsonHandler handler) { return _impl->engine.onJson(controlId, handler); }

bool IoTLabs::setPinMapping(const char* controlId, uint8_t pin, DigitalOutTag, bool activeLow) {
  return PinMappingRegistry::instance().addDigitalOut(_impl->engine, controlId, pin, activeLow);
}

bool IoTLabs::setPinMapping(const char* controlId, uint8_t pin, DigitalInDebouncedTag, bool activeLow,
                             uint32_t debounceMs, const char* reportValueOnRelease) {
  return PinMappingRegistry::instance().addDigitalIn(controlId, pin, activeLow, debounceMs,
                                                       reportValueOnRelease);
}

bool IoTLabs::setPinMapping(const char* controlId, uint8_t pin, AnalogInTag, float rawMin, float rawMax,
                             float scaledMin, float scaledMax, uint32_t pollIntervalMs,
                             float changeThreshold) {
  return PinMappingRegistry::instance().addAnalogIn(controlId, pin, rawMin, rawMax, scaledMin, scaledMax,
                                                      pollIntervalMs, changeThreshold);
}

bool IoTLabs::setPinMapping(const char* controlId, uint8_t pin, PwmOutTag, bool activeLow, uint16_t pwmMin,
                             uint16_t pwmMax) {
  return PinMappingRegistry::instance().addPwmOut(_impl->engine, controlId, pin, activeLow, pwmMin, pwmMax);
}

bool IoTLabs::report(const char* controlId, bool value, bool publishNow) {
  return _impl->engine.reportBool(controlId, value, publishNow);
}

bool IoTLabs::report(const char* controlId, int value, bool publishNow) {
  return report(controlId, static_cast<long>(value), publishNow);
}

bool IoTLabs::report(const char* controlId, long value, bool publishNow) {
  return _impl->engine.reportInt(controlId, value, publishNow);
}

bool IoTLabs::report(const char* controlId, float value, bool publishNow) {
  return report(controlId, static_cast<double>(value), publishNow);
}

bool IoTLabs::report(const char* controlId, double value, bool publishNow) {
  return _impl->engine.reportNumber(controlId, value, publishNow);
}

bool IoTLabs::report(const char* controlId, const char* value, bool publishNow) {
  return report(controlId, String(value ? value : ""), publishNow);
}

bool IoTLabs::report(const char* controlId, const String& value, bool publishNow) {
  return _impl->engine.reportString(controlId, value.c_str(), publishNow);
}

bool IoTLabs::report(const char* controlId, JsonVariantConst value, bool publishNow) {
  return _impl->engine.reportJson(controlId, value, publishNow);
}

bool IoTLabs::connected() const { return _impl->engine.connected(); }
bool IoTLabs::wifiConnected() const { return _impl->engine.wifiConnected(); }
bool IoTLabs::provisioned() const { return _impl->engine.provisioned(); }
bool IoTLabs::provisioning() const { return _impl->engine.provisioning(); }
IoTLabsStatus IoTLabs::status() const { return _impl->engine.status(); }

void IoTLabs::onEvent(EventHandler handler) {
  _impl->engine.onEvent(handler);
  // Also cached here: PinMappingRegistry's strapping-pin warning fires from
  // the facade layer (IoTLabsEngine::_eventHandler is private, with no
  // accessor for a sibling component to reach) and would otherwise never
  // reach the sketch's onEvent() callback.
  PinMappingRegistry::instance().setEventHandler(handler);
}

// Deliberate scoping decision, not an oversight: unlike onEvent() above,
// onCommandFinish() has no PinMappingRegistry interaction. That routing
// exists specifically for auto-pin-mapping's strapping-pin warning, which
// has no analogue for a per-command "dispatch finished" hook.
void IoTLabs::onCommandFinish(CommandFinishHandler handler) { _impl->engine.onCommandFinish(handler); }

bool IoTLabs::startProvisioning() { return _impl->engine.startProvisioning(); }
bool IoTLabs::resetWiFi() { return _impl->engine.resetWiFi(); }
bool IoTLabs::resetIdentity() { return _impl->engine.resetIdentity(); }
bool IoTLabs::factoryReset(bool restartDevice) { return _impl->engine.factoryReset(restartDevice); }

bool IoTLabs::publishTelemetrySnapshot() { return _impl->engine.publishTelemetrySnapshot(); }
bool IoTLabs::publishStatus() { return _impl->engine.publishStatus(); }

String IoTLabs::hardwareId() const { return String(_impl->engine.hardwareId()); }
String IoTLabs::deviceId() const { return String(_impl->engine.deviceId()); }
String IoTLabs::deviceTemplateId() const { return String(_impl->engine.deviceTemplateId()); }
uint32_t IoTLabs::deviceTemplateVersion() const { return _impl->engine.deviceTemplateVersion(); }
String IoTLabs::provisioningApName() const { return String(_impl->engine.provisioningApName()); }
String IoTLabs::lastError() const { return String(_impl->engine.lastError()); }

int IoTLabs::mqttState() const { return _impl->engine.mqttState(); }
uint32_t IoTLabs::reconnectDelayMs() const { return _impl->engine.reconnectDelayMs(); }
