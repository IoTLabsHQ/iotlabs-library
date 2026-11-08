#pragma once

#include <cstdint>

// Pure decision logic behind iot.setPinMapping() (src/PinMappingRegistry.h)
// — debounce, poll-interval + change-threshold gating, and output level/duty
// math. Deliberately has zero GPIO/Arduino calls so it stays host-testable
// the same way ControlRouter/StateRegistry are (IoTLabs internal design notes
// ): callers feed it raw
// digitalRead()/analogRead()/millis() samples and it decides what, if
// anything, should be reported. The real GPIO/report glue (and the write-side
// ControlRouter trampoline registration) lives only in the Arduino-facing
// src/PinMappingRegistry.h/.cpp, which owns one PinMappingLogic per mapping.
class PinMappingLogic {
 public:
  // --- Digital input (button/trigger or state switch), debounced ---
  struct DigitalInConfig {
    bool activeLow = false;
    uint32_t debounceMs = 50;
  };
  void configureDigitalIn(const DigitalInConfig& cfg) { _digitalInCfg = cfg; }

  // Pre-seeds the debounced baseline from one eager reading taken at
  // registration time, with no debounce delay and no edge emitted --
  // mirrors examples/07_Smart_Home_Complete_Demo_Console's setup() doing
  // `buttonLastRaw = digitalRead(pin); buttonStable = buttonLastRaw;` before
  // the first loop() tick, so a real button state at boot is captured
  // immediately instead of only becoming trustworthy after the first
  // debounce window elapses.
  void seedInitialState(bool rawHigh, uint32_t nowMs);

  enum class Edge : uint8_t { NONE, BECAME_ACTIVE, BECAME_INACTIVE };

  // Feed a raw digitalRead() result (true = HIGH) and millis(). Reproduces
  // examples/07_Smart_Home_Complete_Demo_Console's updateBootButton() timing
  // exactly: a raw-level change resets the debounce timer; once the timer has
  // elapsed AND the debounced level actually differs from the last committed
  // one, the new level commits and the edge is reported. `outActiveState`
  // always receives the current debounced active-state (post activeLow),
  // regardless of which edge (or no edge) fired this call.
  Edge feedDigitalSample(bool rawHigh, uint32_t nowMs, bool& outActiveState);

  // --- Analog input (raw sensor), scaled + change-thresholded ---
  struct AnalogInConfig {
    float rawMin = 0;
    float rawMax = 4095;
    float scaledMin = 0;
    float scaledMax = 100;
    uint32_t pollIntervalMs = 200;
    float changeThreshold = 0;
  };
  void configureAnalogIn(const AnalogInConfig& cfg) { _analogInCfg = cfg; }

  // Feed a raw analogRead() result and millis(). Returns true and sets
  // outValue (scaled to [scaledMin,scaledMax]) when it's time to report: the
  // poll interval has elapsed AND |delta since the last reported value| >=
  // changeThreshold (the very first sample always reports, so a fresh
  // mapping isn't silent until the value happens to move).
  bool feedAnalogSample(int rawValue, uint32_t nowMs, float& outValue);

  // --- Digital/PWM output (stateless, pure math -- no debounce needed) ---

  // The physical HIGH/LOW level that should be written for a logical
  // command value, honoring activeLow.
  static bool outputLevelHigh(bool commandActive, bool activeLow) {
    return activeLow ? !commandActive : commandActive;
  }

  // Maps a 0-100 logical duty into [pwmMin,pwmMax], honoring activeLow by
  // inverting around the same range (an active-LOW PWM driver's "0% logical"
  // is its numeric max, not 0).
  static uint16_t outputDuty(float commandPercent0to100, uint16_t pwmMin, uint16_t pwmMax, bool activeLow);

 private:
  DigitalInConfig _digitalInCfg;
  bool _lastRaw = false;
  bool _hasLastRaw = false;
  bool _stableActive = false;
  bool _hasStable = false;
  uint32_t _lastChangeAt = 0;

  AnalogInConfig _analogInCfg;
  bool _hasReportedOnce = false;
  float _lastReportedValue = 0;
  uint32_t _lastPollAt = 0;
  bool _hasPolledOnce = false;
};
