#include "ControlRouter.h"

#include <cctype>

ControlRouter::HandlerEntry* ControlRouter::find(const std::string& controlId) {
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_handlers[i].used && _handlers[i].id == controlId) return &_handlers[i];
  }
  return nullptr;
}

ControlRouter::HandlerEntry* ControlRouter::findOrCreate(const std::string& controlId) {
  HandlerEntry* existing = find(controlId);
  if (existing) return existing;
  for (uint8_t i = 0; i < MAX_CONTROLS; ++i) {
    if (_handlers[i].used) continue;
    _handlers[i].used = true;
    _handlers[i].id = controlId;
    return &_handlers[i];
  }
  return nullptr;
}

bool ControlRouter::on(const char* controlId, BoolHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreate(std::string(controlId));
  if (!entry) return false;
  entry->kind = Kind::BOOL;
  entry->boolHandler = handler;
  return true;
}

bool ControlRouter::on(const char* controlId, NumberHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreate(std::string(controlId));
  if (!entry) return false;
  entry->kind = Kind::NUMBER;
  entry->numberHandler = handler;
  return true;
}

bool ControlRouter::on(const char* controlId, StringHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreate(std::string(controlId));
  if (!entry) return false;
  entry->kind = Kind::STRING;
  entry->stringHandler = handler;
  return true;
}

bool ControlRouter::on(const char* controlId, JsonHandler handler) {
  if (!controlId || !handler) return false;
  HandlerEntry* entry = findOrCreate(std::string(controlId));
  if (!entry) return false;
  entry->kind = Kind::JSON;
  entry->jsonHandler = handler;
  return true;
}

bool ControlRouter::parseBool(JsonVariantConst value, bool& outResult) {
  if (value.is<bool>()) {
    outResult = value.as<bool>();
    return true;
  }
  if (value.is<int>() || value.is<long>()) {
    outResult = value.as<long>() != 0;
    return true;
  }
  if (value.is<const char*>()) {
    std::string s(value.as<const char*>());
    for (char& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (s == "on" || s == "true" || s == "1") {
      outResult = true;
      return true;
    }
    if (s == "off" || s == "false" || s == "0") {
      outResult = false;
      return true;
    }
  }
  return false;
}

bool ControlRouter::dispatch(const char* controlId, JsonVariantConst value, bool& outHandlerFound) {
  outHandlerFound = false;
  HandlerEntry* handler = find(controlId ? std::string(controlId) : std::string());
  if (!handler) return false;
  outHandlerFound = true;

  switch (handler->kind) {
    case Kind::BOOL: {
      bool parsed = false;
      if (!parseBool(value, parsed)) return false;
      if (handler->boolHandler) handler->boolHandler(parsed);
      return true;
    }
    case Kind::NUMBER:
      if (!(value.is<int>() || value.is<long>() || value.is<float>() || value.is<double>())) return false;
      if (handler->numberHandler) handler->numberHandler(value.as<float>());
      return true;
    case Kind::STRING:
      if (!value.is<const char*>()) return false;
      if (handler->stringHandler) handler->stringHandler(String(value.as<const char*>()));
      return true;
    case Kind::JSON:
      if (handler->jsonHandler) handler->jsonHandler(IoTLabsValue(value));
      return true;
    default:
      return false;
  }
}
