#pragma once

// See src/IoTLabs.h's own guard comment -- same rule applies here: this is
// the Arduino-facing GPIO glue behind iot.setPinMapping(), never built for
// native/host targets (which drive src/core/IoTLabsEngine.h directly and
// never see this file).
#if !defined(ARDUINO)
#error "src/PinMappingRegistry.h requires an Arduino-compatible core -- see src/IoTLabs.h."
#endif

#include <Arduino.h>
#include <cstdint>
#include <string>

#include "IoTLabsTypes.h"
#include "core/ControlRouter.h"
#include "core/IoTLabsEngine.h"
#include "core/PinMappingLogic.h"

// Arduino-facing GPIO glue behind iot.setPinMapping(): the real
// pinMode()/digitalRead()/digitalWrite()/analogRead()/analogWrite()/millis()
// calls, plus the write-side ControlRouter handler registration. Every
// debounce/scale/threshold DECISION is delegated to PinMappingLogic
// (src/core/PinMappingLogic.h), which stays Arduino-free and is unit-tested
// on host (IoTLabs internal design notes
// ) -- this class itself is verified only via
// arduino-cli compile + real hardware, same as the rest of src/IoTLabs.cpp.
class PinMappingRegistry {
 public:
  static constexpr uint8_t MAX_PIN_MAPPINGS = ControlRouter::MAX_CONTROLS;

  // The one deliberate singleton in this codebase: ControlRouter's on()
  // handlers are raw C function pointers with no userdata/context parameter
  // (BoolHandler = void(*)(bool)), so the write-side trampolines
  // (PinMappingRegistry.cpp) are free functions that can't capture `this` --
  // they reach the live registry through here instead.
  static PinMappingRegistry& instance();

  bool addDigitalOut(IoTLabsEngine& engine, const char* controlId, uint8_t pin, bool activeLow);
  bool addDigitalIn(const char* controlId, uint8_t pin, bool activeLow, uint32_t debounceMs,
                     const char* reportValueOnRelease);
  bool addAnalogIn(const char* controlId, uint8_t pin, float rawMin, float rawMax, float scaledMin,
                    float scaledMax, uint32_t pollIntervalMs, float changeThreshold);
  bool addPwmOut(IoTLabsEngine& engine, const char* controlId, uint8_t pin, bool activeLow, uint16_t pwmMin,
                  uint16_t pwmMax);

  void setEventHandler(void (*handler)(IoTLabsEvent)) { _eventHandler = handler; }

  // Called from IoTLabs::loop(): polls every DIGITAL_IN_DEBOUNCED/ANALOG_IN
  // mapping and reports through `engine` when their logic says to.
  // DIGITAL_OUT/PWM_OUT mappings are write-driven (via the trampolines
  // below, on command arrival) -- nothing to do for them here.
  void loop(IoTLabsEngine& engine);

  // Invoked only by the compile-time-generated trampolines in
  // PinMappingRegistry.cpp -- not part of the intended call surface
  // otherwise.
  void applyBoolOutput(uint8_t slot, bool value);
  void applyNumberOutput(uint8_t slot, float value);

 private:
  enum class Kind : uint8_t { DIGITAL_OUT, DIGITAL_IN_DEBOUNCED, ANALOG_IN, PWM_OUT };

  struct Slot {
    bool used = false;
    Kind kind = Kind::DIGITAL_OUT;
    uint8_t pin = 0;
    std::string controlId;
    bool activeLow = false;
    std::string reportValueOnRelease;  // DIGITAL_IN_DEBOUNCED only; empty = report bool state instead
    uint16_t pwmMin = 0;
    uint16_t pwmMax = 255;
    PinMappingLogic logic;
  };

  Slot _slots[MAX_PIN_MAPPINGS];
  void (*_eventHandler)(IoTLabsEvent) = nullptr;
  // Set on every addDigitalOut()/addPwmOut()/loop() call (there's only ever
  // one IoTLabsEngine per sketch in practice). Needed so applyBoolOutput()/
  // applyNumberOutput() -- invoked from trampolines whose signature carries
  // no engine reference -- can still echo the applied state back via
  // report(), matching examples/01's setRelay() pattern (write, then report
  // the actual hardware state).
  IoTLabsEngine* _engine = nullptr;

  int8_t findFreeSlot() const;
  void checkStrappingPin(uint8_t pin) const;
};
