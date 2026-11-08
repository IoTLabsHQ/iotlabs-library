#include "StateRegistry.h"

StateRegistry::StateEntry* StateRegistry::find(const std::string& controlId) {
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_states[i].used && _states[i].id == controlId) return &_states[i];
  }
  return nullptr;
}

StateRegistry::StateEntry* StateRegistry::findOrCreate(const std::string& controlId) {
  StateEntry* existing = find(controlId);
  if (existing) return existing;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_states[i].used) continue;
    _states[i].used = true;
    _states[i].id = controlId;
    return &_states[i];
  }
  return nullptr;
}

bool StateRegistry::reportBool(const char* controlId, bool value) {
  if (!controlId) return false;
  StateEntry* state = findOrCreate(std::string(controlId));
  if (!state) return false;
  state->kind = Kind::BOOL;
  state->boolValue = value;
  return true;
}

bool StateRegistry::reportInt(const char* controlId, long value) {
  if (!controlId) return false;
  StateEntry* state = findOrCreate(std::string(controlId));
  if (!state) return false;
  state->kind = Kind::INTEGER;
  state->intValue = value;
  return true;
}

bool StateRegistry::reportNumber(const char* controlId, double value) {
  if (!controlId) return false;
  StateEntry* state = findOrCreate(std::string(controlId));
  if (!state) return false;
  state->kind = Kind::NUMBER;
  state->numberValue = value;
  return true;
}

bool StateRegistry::reportString(const char* controlId, const std::string& value) {
  if (!controlId) return false;
  StateEntry* state = findOrCreate(std::string(controlId));
  if (!state) return false;
  state->kind = Kind::STRING;
  state->stringValue = value;
  return true;
}

bool StateRegistry::reportJson(const char* controlId, JsonVariantConst value) {
  if (!controlId) return false;
  StateEntry* state = findOrCreate(std::string(controlId));
  if (!state) return false;
  state->kind = Kind::JSON;
  state->jsonValue.clear();
  serializeJson(value, state->jsonValue);
  return true;
}

void StateRegistry::append(JsonDocument& doc, const StateEntry& state) {
  switch (state.kind) {
    case Kind::BOOL:
      doc[state.id] = state.boolValue ? "on" : "off";
      break;
    case Kind::INTEGER:
      doc[state.id] = state.intValue;
      break;
    case Kind::NUMBER:
      doc[state.id] = state.numberValue;
      break;
    case Kind::STRING:
      doc[state.id] = state.stringValue;
      break;
    case Kind::JSON: {
      JsonDocument nested;
      if (!deserializeJson(nested, state.jsonValue)) doc[state.id] = nested.as<JsonVariantConst>();
      break;
    }
    default:
      break;
  }
}

bool StateRegistry::appendAll(JsonDocument& doc) const {
  bool hasState = false;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (!_states[i].used || _states[i].kind == Kind::NONE) continue;
    append(doc, _states[i]);
    hasState = true;
  }
  return hasState;
}

bool StateRegistry::getNumber(const char* controlId, double& outValue) const {
  if (!controlId) return false;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (!_states[i].used || _states[i].id != controlId) continue;
    if (_states[i].kind == Kind::INTEGER) {
      outValue = static_cast<double>(_states[i].intValue);
      return true;
    }
    if (_states[i].kind == Kind::NUMBER) {
      outValue = _states[i].numberValue;
      return true;
    }
    return false;
  }
  return false;
}

bool StateRegistry::appendOne(JsonDocument& doc, const char* controlId) const {
  if (!controlId) return false;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_states[i].used && _states[i].id == controlId && _states[i].kind != Kind::NONE) {
      append(doc, _states[i]);
      return true;
    }
  }
  return false;
}
