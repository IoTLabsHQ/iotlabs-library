#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// ArduinoJson is treated as an accepted portable dependency for the wire/value
// layer (it targets the same architectures IoTLabs targets, including native
// builds), unlike WiFiClient/Preferences/PubSubClient/HTTPClient which are
// ESP32/Arduino-networking-specific and must stay behind adapters. See
// IoTLabs internal design notes
class IoTLabsValue {
 public:
  explicit IoTLabsValue(JsonVariantConst value) : _value(value) {}

  bool isNull() const { return _value.isNull(); }
  bool isBool() const { return _value.is<bool>(); }
  bool isNumber() const {
    return _value.is<int>() || _value.is<long>() || _value.is<float>() || _value.is<double>();
  }
  bool isString() const { return _value.is<const char*>(); }
  bool isObject() const { return _value.is<JsonObjectConst>(); }
  bool isArray() const { return _value.is<JsonArrayConst>(); }

  bool asBool(bool fallback = false) const;
  long asInt(long fallback = 0) const;
  float asFloat(float fallback = 0.0f) const;
  String asString(const String& fallback = String()) const;

  bool has(const char* key) const;
  IoTLabsValue operator[](const char* key) const;

  JsonVariantConst raw() const { return _value; }

 private:
  JsonVariantConst _value;
};
