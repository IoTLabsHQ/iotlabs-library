#include "PinMappingRegistry.h"

#include <utility>

namespace {

// Compile-time-generated write-side trampolines -- see PinMappingRegistry.h
// for why these have to be free functions instead of capturing lambdas.
template <uint8_t N>
void boolTrampoline(bool v) {
  PinMappingRegistry::instance().applyBoolOutput(N, v);
}

template <uint8_t N>
void numberTrampoline(float v) {
  PinMappingRegistry::instance().applyNumberOutput(N, v);
}

template <uint8_t... I>
void fillBoolTrampolines(ControlRouter::BoolHandler* table, std::integer_sequence<uint8_t, I...>) {
  ((table[I] = &boolTrampoline<I>), ...);
}

template <uint8_t... I>
void fillNumberTrampolines(ControlRouter::NumberHandler* table, std::integer_sequence<uint8_t, I...>) {
  ((table[I] = &numberTrampoline<I>), ...);
}

ControlRouter::BoolHandler g_boolTrampolines[PinMappingRegistry::MAX_PIN_MAPPINGS];
ControlRouter::NumberHandler g_numberTrampolines[PinMappingRegistry::MAX_PIN_MAPPINGS];

struct TrampolineInit {
  TrampolineInit() {
    fillBoolTrampolines(g_boolTrampolines,
                         std::make_integer_sequence<uint8_t, PinMappingRegistry::MAX_PIN_MAPPINGS>{});
    fillNumberTrampolines(g_numberTrampolines,
                           std::make_integer_sequence<uint8_t, PinMappingRegistry::MAX_PIN_MAPPINGS>{});
  }
} g_trampolineInit;

// Classic ESP32 boot-strapping GPIOs only -- S2/S3/C3 variants use a
// different set. Advisory warning, not a hard block; a documented follow-up
// to extend this per-variant.
constexpr uint8_t kStrappingPins[] = {0, 2, 12, 15};

}  // namespace

PinMappingRegistry& PinMappingRegistry::instance() {
  static PinMappingRegistry reg;
  return reg;
}

int8_t PinMappingRegistry::findFreeSlot() const {
  for (uint8_t i = 0; i < MAX_PIN_MAPPINGS; ++i) {
    if (!_slots[i].used) return static_cast<int8_t>(i);
  }
  return -1;
}

void PinMappingRegistry::checkStrappingPin(uint8_t pin) const {
  if (!_eventHandler) return;
  for (uint8_t p : kStrappingPins) {
    if (p == pin) {
      _eventHandler(IoTLabsEvent::PIN_MAPPING_STRAPPING_WARNING);
      return;
    }
  }
}

bool PinMappingRegistry::addDigitalOut(IoTLabsEngine& engine, const char* controlId, uint8_t pin,
                                        bool activeLow) {
  if (!controlId) return false;
  const int8_t slot = findFreeSlot();
  if (slot < 0) return false;
  checkStrappingPin(pin);

  Slot& s = _slots[slot];
  s.used = true;
  s.kind = Kind::DIGITAL_OUT;
  s.pin = pin;
  s.controlId = controlId;
  s.activeLow = activeLow;

  // Boot-glitch-safe init: latch the off-level via digitalWrite() BEFORE
  // pinMode(OUTPUT) commits the pin as an output -- Arduino allows presetting
  // the output-latch level even before OUTPUT mode is set, which avoids a
  // transient glitch window on ESP32 reset/flash where the pin could
  // otherwise float/settle at an undefined level for a moment.
  const bool offHigh = PinMappingLogic::outputLevelHigh(/*commandActive*/ false, activeLow);
  digitalWrite(pin, offHigh ? HIGH : LOW);
  pinMode(pin, OUTPUT);

  _engine = &engine;
  // Cache the safe initial state without publishing yet -- matches
  // examples/01's `iot.report(CONTROL_RELAY, false, false)` done right
  // after the safe-boot digitalWrite, before iot.begin() connects.
  engine.reportBool(controlId, false, /*publishNow*/ false);

  return engine.onBool(controlId, g_boolTrampolines[slot]);
}

bool PinMappingRegistry::addDigitalIn(const char* controlId, uint8_t pin, bool activeLow,
                                       uint32_t debounceMs, const char* reportValueOnRelease) {
  if (!controlId) return false;
  const int8_t slot = findFreeSlot();
  if (slot < 0) return false;
  checkStrappingPin(pin);

  Slot& s = _slots[slot];
  s.used = true;
  s.kind = Kind::DIGITAL_IN_DEBOUNCED;
  s.pin = pin;
  s.controlId = controlId;
  s.activeLow = activeLow;
  s.reportValueOnRelease = reportValueOnRelease ? reportValueOnRelease : "";
  s.logic.configureDigitalIn({activeLow, debounceMs});

  // activeLow implies the idle level needs an internal pull-up (e.g. a
  // button wired to GND) -- matches examples/07's BOOT_BUTTON_PIN wiring
  // exactly. activeLow=false assumes external pull-down/driven-HIGH wiring.
  pinMode(pin, activeLow ? INPUT_PULLUP : INPUT);
  s.logic.seedInitialState(digitalRead(pin) == HIGH, millis());
  return true;
}

bool PinMappingRegistry::addAnalogIn(const char* controlId, uint8_t pin, float rawMin, float rawMax,
                                      float scaledMin, float scaledMax, uint32_t pollIntervalMs,
                                      float changeThreshold) {
  if (!controlId) return false;
  const int8_t slot = findFreeSlot();
  if (slot < 0) return false;

  Slot& s = _slots[slot];
  s.used = true;
  s.kind = Kind::ANALOG_IN;
  s.pin = pin;
  s.controlId = controlId;
  s.logic.configureAnalogIn({rawMin, rawMax, scaledMin, scaledMax, pollIntervalMs, changeThreshold});

  pinMode(pin, INPUT);
  return true;
}

bool PinMappingRegistry::addPwmOut(IoTLabsEngine& engine, const char* controlId, uint8_t pin, bool activeLow,
                                    uint16_t pwmMin, uint16_t pwmMax) {
  if (!controlId) return false;
  const int8_t slot = findFreeSlot();
  if (slot < 0) return false;
  checkStrappingPin(pin);

  Slot& s = _slots[slot];
  s.used = true;
  s.kind = Kind::PWM_OUT;
  s.pin = pin;
  s.controlId = controlId;
  s.activeLow = activeLow;
  s.pwmMin = pwmMin;
  s.pwmMax = pwmMax;

  pinMode(pin, OUTPUT);
  analogWrite(pin, PinMappingLogic::outputDuty(0, pwmMin, pwmMax, activeLow));

  _engine = &engine;
  engine.reportNumber(controlId, 0, /*publishNow*/ false);

  return engine.onNumber(controlId, g_numberTrampolines[slot]);
}

void PinMappingRegistry::applyBoolOutput(uint8_t slot, bool value) {
  if (slot >= MAX_PIN_MAPPINGS || !_slots[slot].used) return;
  Slot& s = _slots[slot];
  const bool level = PinMappingLogic::outputLevelHigh(value, s.activeLow);
  digitalWrite(s.pin, level ? HIGH : LOW);
  // Echo the applied hardware state back -- matches examples/01's
  // setRelay()/onRelayControl() pattern (write, then report the actual
  // state), not just a silent digitalWrite.
  if (_engine) _engine->reportBool(s.controlId.c_str(), value, true);
}

void PinMappingRegistry::applyNumberOutput(uint8_t slot, float value) {
  if (slot >= MAX_PIN_MAPPINGS || !_slots[slot].used) return;
  Slot& s = _slots[slot];
  analogWrite(s.pin, PinMappingLogic::outputDuty(value, s.pwmMin, s.pwmMax, s.activeLow));
  if (_engine) _engine->reportNumber(s.controlId.c_str(), value, true);
}

void PinMappingRegistry::loop(IoTLabsEngine& engine) {
  _engine = &engine;
  const uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_PIN_MAPPINGS; ++i) {
    Slot& s = _slots[i];
    if (!s.used) continue;

    if (s.kind == Kind::DIGITAL_IN_DEBOUNCED) {
      const bool rawHigh = digitalRead(s.pin) == HIGH;
      bool activeState = false;
      const PinMappingLogic::Edge edge = s.logic.feedDigitalSample(rawHigh, now, activeState);
      if (edge == PinMappingLogic::Edge::NONE) continue;

      if (!s.reportValueOnRelease.empty()) {
        if (edge == PinMappingLogic::Edge::BECAME_INACTIVE) {
          engine.reportString(s.controlId.c_str(), s.reportValueOnRelease.c_str(), true);
        }
      } else {
        engine.reportBool(s.controlId.c_str(), activeState, true);
      }
    } else if (s.kind == Kind::ANALOG_IN) {
      const int raw = analogRead(s.pin);
      float value = 0;
      if (s.logic.feedAnalogSample(raw, now, value)) {
        engine.reportNumber(s.controlId.c_str(), value, true);
      }
    }
    // DIGITAL_OUT/PWM_OUT: write-driven via the trampolines above, nothing
    // to poll here.
  }
}
