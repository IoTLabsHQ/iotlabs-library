#pragma once

#include <ArduinoJson.h>
#include <cstdint>
#include <string>

// Bounded reported-state cache (fixed MAX_CONTROLS slots, see
// IoTLabs internal design notes) plus the JSON
// shaping shared by single-state telemetry, snapshot telemetry and the
// status payload's inline state block.
class StateRegistry {
 public:
  static constexpr uint8_t MAX_CONTROLS = 24;

  enum class Kind : uint8_t { NONE, BOOL, INTEGER, NUMBER, STRING, JSON };

  bool reportBool(const char* controlId, bool value);
  bool reportInt(const char* controlId, long value);
  bool reportNumber(const char* controlId, double value);
  bool reportString(const char* controlId, const std::string& value);
  bool reportJson(const char* controlId, JsonVariantConst value);

  // Returns true if any state slot is populated.
  bool appendAll(JsonDocument& doc) const;
  bool appendOne(JsonDocument& doc, const char* controlId) const;

  // Reads back the last reported value as a number, for AutomationEngine's
  // local condition evaluation — works for either reportInt() or
  // reportNumber() (both are numeric), false for any other kind or an
  // unknown/never-reported controlId.
  bool getNumber(const char* controlId, double& outValue) const;

 private:
  struct StateEntry {
    bool used = false;
    std::string id;
    Kind kind = Kind::NONE;
    bool boolValue = false;
    long intValue = 0;
    double numberValue = 0.0;
    std::string stringValue;
    std::string jsonValue;
  };

  StateEntry _states[MAX_CONTROLS];

  StateEntry* find(const std::string& controlId);
  StateEntry* findOrCreate(const std::string& controlId);
  static void append(JsonDocument& doc, const StateEntry& state);
};
