#include "IoTLabsStorage.h"

bool IoTLabsStorage::begin(const char* ns) {
  if (_opened) return true;
  if (!ns || strlen(ns) == 0) return false;
  _opened = _prefs.begin(ns, false);
  return _opened;
}

void IoTLabsStorage::end() {
  if (!_opened) return;
  _prefs.end();
  _opened = false;
}

bool IoTLabsStorage::hasWifi() {
  return _opened && _prefs.getString("wifi_ssid", "").length() > 0;
}

String IoTLabsStorage::wifiSsid() {
  return _opened ? _prefs.getString("wifi_ssid", "") : String();
}

String IoTLabsStorage::wifiPassword() {
  return _opened ? _prefs.getString("wifi_pass", "") : String();
}

bool IoTLabsStorage::saveWifi(const String& ssid, const String& password) {
  if (!_opened || ssid.length() == 0) return false;
  return _prefs.putString("wifi_ssid", ssid) > 0 &&
         _prefs.putString("wifi_pass", password) >= 0;
}

bool IoTLabsStorage::clearWifi() {
  if (!_opened) return false;
  _prefs.remove("wifi_ssid");
  _prefs.remove("wifi_pass");
  return true;
}

String IoTLabsStorage::activationToken() {
  return _opened ? _prefs.getString("act_token", "") : String();
}

bool IoTLabsStorage::saveActivationToken(const String& token) {
  if (!_opened || token.length() == 0) return false;
  return _prefs.putString("act_token", token) > 0;
}

bool IoTLabsStorage::clearActivationToken() {
  if (!_opened) return false;
  _prefs.remove("act_token");
  return true;
}

IoTLabsRuntimeIdentity IoTLabsStorage::runtimeIdentity() {
  IoTLabsRuntimeIdentity out;
  if (!_opened) return out;

  out.deviceId = _prefs.getString("device_id", "");
  out.profileId = _prefs.getString("profile_id", "");
  out.profileVersion = _prefs.getUInt("profile_ver", 1);
  out.mqttClientId = _prefs.getString("mqtt_cid", "");
  out.mqttUsername = _prefs.getString("mqtt_user", "");
  out.mqttPassword = _prefs.getString("mqtt_pass", "");
  out.mqttBroker = _prefs.getString("mqtt_host", "");
  out.mqttPort = _prefs.getUShort("mqtt_port", 8883);
  out.topicPrefix = _prefs.getString("topic_pref", "");
  return out;
}

bool IoTLabsStorage::saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) {
  if (!_opened || !identity.valid()) return false;

  _prefs.putString("device_id", identity.deviceId);
  _prefs.putString("profile_id", identity.profileId);
  _prefs.putUInt("profile_ver", identity.profileVersion);
  _prefs.putString("mqtt_cid", identity.mqttClientId);
  _prefs.putString("mqtt_user", identity.mqttUsername);
  _prefs.putString("mqtt_pass", identity.mqttPassword);
  _prefs.putString("mqtt_host", identity.mqttBroker);
  _prefs.putUShort("mqtt_port", identity.mqttPort);
  _prefs.putString("topic_pref", identity.topicPrefix);
  return true;
}

bool IoTLabsStorage::clearRuntimeIdentity() {
  if (!_opened) return false;
  const char* keys[] = {
    "device_id", "profile_id", "profile_ver", "mqtt_cid", "mqtt_user",
    "mqtt_pass", "mqtt_host", "mqtt_port", "topic_pref"
  };
  for (const char* key : keys) _prefs.remove(key);
  return true;
}

bool IoTLabsStorage::clearAll() {
  if (!_opened) return false;
  return _prefs.clear();
}
