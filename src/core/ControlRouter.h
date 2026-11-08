#pragma once

#include <Arduino.h>
#include <cstdint>
#include <string>

#include "../IoTLabsValue.h"

// Registry of application callbacks keyed by controlId, and the dispatcher
// that routes an incoming cmd payload to the right one. Fixed-capacity by
// design (IoTLabs internal design notes: bounded
// control/state caches, no unbounded allocation in the command hot path).
class ControlRouter {
 public:
  static constexpr uint8_t MAX_CONTROLS = 24;

  using BoolHandler = void (*)(bool value);
  using NumberHandler = void (*)(float value);
  using StringHandler = void (*)(const String& value);
  using JsonHandler = void (*)(const IoTLabsValue& value);

  enum class Kind : uint8_t { NONE, BOOL, NUMBER, STRING, JSON };

  bool on(const char* controlId, BoolHandler handler);
  bool on(const char* controlId, NumberHandler handler);
  bool on(const char* controlId, StringHandler handler);
  bool on(const char* controlId, JsonHandler handler);

  // Returns false if controlId is unknown; outConsumed reports whether the
  // handler kind matched the payload well enough to invoke a callback.
  bool dispatch(const char* controlId, JsonVariantConst value, bool& outHandlerFound);

  static bool parseBool(JsonVariantConst value, bool& outResult);

 private:
  struct HandlerEntry {
    bool used = false;
    std::string id;
    Kind kind = Kind::NONE;
    BoolHandler boolHandler = nullptr;
    NumberHandler numberHandler = nullptr;
    StringHandler stringHandler = nullptr;
    JsonHandler jsonHandler = nullptr;
  };

  HandlerEntry _handlers[MAX_CONTROLS];

  HandlerEntry* find(const std::string& controlId);
  HandlerEntry* findOrCreate(const std::string& controlId);
};
