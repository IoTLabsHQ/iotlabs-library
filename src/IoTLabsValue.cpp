#include "IoTLabsValue.h"

bool IoTLabsValue::asBool(bool fallback) const {
  if (_value.is<bool>()) return _value.as<bool>();
  if (_value.is<int>() || _value.is<long>()) return _value.as<long>() != 0;
  if (_value.is<const char*>()) {
    String s = _value.as<const char*>();
    s.toLowerCase();
    if (s == "on" || s == "true" || s == "1") return true;
    if (s == "off" || s == "false" || s == "0") return false;
  }
  return fallback;
}

long IoTLabsValue::asInt(long fallback) const {
  if (_value.is<long>() || _value.is<int>()) return _value.as<long>();
  if (_value.is<float>() || _value.is<double>()) return static_cast<long>(_value.as<double>());
  return fallback;
}

float IoTLabsValue::asFloat(float fallback) const {
  if (_value.is<float>() || _value.is<double>() || _value.is<long>() || _value.is<int>()) {
    return _value.as<float>();
  }
  return fallback;
}

String IoTLabsValue::asString(const String& fallback) const {
  if (_value.is<const char*>()) return String(_value.as<const char*>());
  return fallback;
}

bool IoTLabsValue::has(const char* key) const {
  if (!_value.is<JsonObjectConst>()) return false;
  return _value.as<JsonObjectConst>().containsKey(key);
}

IoTLabsValue IoTLabsValue::operator[](const char* key) const {
  if (!_value.is<JsonObjectConst>()) return IoTLabsValue(JsonVariantConst());
  return IoTLabsValue(_value.as<JsonObjectConst>()[key]);
}
