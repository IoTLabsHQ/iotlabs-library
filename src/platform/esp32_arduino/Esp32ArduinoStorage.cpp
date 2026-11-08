#include "Esp32ArduinoStorage.h"

#if defined(ARDUINO_ARCH_ESP32)

bool Esp32ArduinoStorage::begin(const char* ns) {
  if (_opened) return true;
  if (!ns || ns[0] == '\0') return false;
  _opened = _prefs.begin(ns, false);
  return _opened;
}

bool Esp32ArduinoStorage::rawGetString(const char* key, std::string& out) const {
  if (!_opened) return false;
  out = std::string(_prefs.getString(key, "").c_str());
  return true;
}

bool Esp32ArduinoStorage::rawPutString(const char* key, const std::string& value) {
  if (!_opened) return false;
  return _prefs.putString(key, value.c_str()) >= 0;
}

bool Esp32ArduinoStorage::rawRemove(const char* key) {
  if (!_opened) return false;
  _prefs.remove(key);
  return true;
}

bool Esp32ArduinoStorage::hasWifi() const {
  if (!_opened) return false;
  return _prefs.getString("wifi_ssid", "").length() > 0;
}

bool Esp32ArduinoStorage::loadWifiSsid(std::string& outSsid) const { return rawGetString("wifi_ssid", outSsid); }

bool Esp32ArduinoStorage::saveWifiSsid(const std::string& ssid) {
  if (ssid.empty()) return false;
  return rawPutString("wifi_ssid", ssid);
}

bool Esp32ArduinoStorage::clearWifi() { return rawRemove("wifi_ssid"); }

bool Esp32ArduinoStorage::loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const {
  if (!_opened) return false;
  outIdentity.deviceId = std::string(_prefs.getString("device_id", "").c_str());
  outIdentity.deviceTemplateId = std::string(_prefs.getString("device_template_id", "").c_str());
  outIdentity.deviceTemplateVersion = _prefs.getUInt("device_template_ver", 1);
  outIdentity.mqttClientId = std::string(_prefs.getString("mqtt_cid", "").c_str());
  outIdentity.mqttUsername = std::string(_prefs.getString("mqtt_user", "").c_str());
  outIdentity.mqttPassword = std::string(_prefs.getString("mqtt_pass", "").c_str());
  outIdentity.mqttBroker = std::string(_prefs.getString("mqtt_host", "").c_str());
  outIdentity.mqttPort = _prefs.getUShort("mqtt_port", 8883);
  outIdentity.topicPrefix = std::string(_prefs.getString("topic_pref", "").c_str());
  return true;
}

bool Esp32ArduinoStorage::saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) {
  if (!_opened || !identity.valid()) return false;

  _prefs.putString("device_id", identity.deviceId.c_str());
  _prefs.putString("device_template_id", identity.deviceTemplateId.c_str());
  _prefs.putUInt("device_template_ver", identity.deviceTemplateVersion);
  _prefs.putString("mqtt_cid", identity.mqttClientId.c_str());
  _prefs.putString("mqtt_user", identity.mqttUsername.c_str());
  _prefs.putString("mqtt_pass", identity.mqttPassword.c_str());
  _prefs.putString("mqtt_host", identity.mqttBroker.c_str());
  _prefs.putUShort("mqtt_port", identity.mqttPort);
  _prefs.putString("topic_pref", identity.topicPrefix.c_str());
  return true;
}

bool Esp32ArduinoStorage::clearRuntimeIdentity() {
  if (!_opened) return false;
  const char* keys[] = {"device_id", "profile_id", "profile_ver", "mqtt_cid", "mqtt_user",
                         "mqtt_pass", "mqtt_host",  "mqtt_port",   "topic_pref"};
  for (const char* key : keys) _prefs.remove(key);
  return true;
}

bool Esp32ArduinoStorage::saveAutomationRules(const std::string& json) { return rawPutString("auto_rules", json); }

bool Esp32ArduinoStorage::loadAutomationRules(std::string& outJson) const { return rawGetString("auto_rules", outJson); }

bool Esp32ArduinoStorage::clearAutomationRules() { return rawRemove("auto_rules"); }

bool Esp32ArduinoStorage::clearAll() {
  if (!_opened) return false;
  return _prefs.clear();
}

#endif  // ARDUINO_ARCH_ESP32
