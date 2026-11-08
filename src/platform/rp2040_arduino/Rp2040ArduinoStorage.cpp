#include "Rp2040ArduinoStorage.h"

#if defined(ARDUINO_ARCH_RP2040)

#include <LittleFS.h>

// Deliberately simple: every key lives in one flat JSON object rewritten in
// full on each write. RP2040/Pico W has no built-in NVS-style key/value store
// the way ESP32's Preferences wraps, so LittleFS + a single JSON document is
// the straightforward "candidate" tier approach (see
// IoTLabs internal design notes) — a future revision
// could move to a real multi-file or append-only layout if write-wear or
// partial-corruption behavior becomes a concern on real hardware.

bool Rp2040ArduinoStorage::begin(const char* ns) {
  if (_opened) return true;
  if (!ns || ns[0] == '\0') return false;
  if (!LittleFS.begin()) return false;
  _ns = ns;
  _opened = true;
  return true;
}

std::string Rp2040ArduinoStorage::filePath() const { return "/" + _ns + ".json"; }

bool Rp2040ArduinoStorage::loadDocument(JsonDocument& doc) const {
  doc.clear();
  if (!_opened) return false;
  File file = LittleFS.open(filePath().c_str(), "r");
  if (!file) return true;  // No file written yet is not an error: treat as empty.
  deserializeJson(doc, file);
  file.close();
  return true;
}

bool Rp2040ArduinoStorage::saveDocument(const JsonDocument& doc) {
  if (!_opened) return false;
  File file = LittleFS.open(filePath().c_str(), "w");
  if (!file) return false;
  const bool ok = serializeJson(doc, file) > 0 || doc.size() == 0;
  file.close();
  return ok;
}

bool Rp2040ArduinoStorage::rawGetString(const char* key, std::string& out) const {
  if (!_opened) return false;
  JsonDocument doc;
  loadDocument(doc);
  const char* value = doc[key] | "";
  out = value;
  return true;
}

bool Rp2040ArduinoStorage::rawPutString(const char* key, const std::string& value) {
  if (!_opened) return false;
  JsonDocument doc;
  loadDocument(doc);
  doc[key] = value.c_str();
  return saveDocument(doc);
}

bool Rp2040ArduinoStorage::rawRemove(const char* key) {
  if (!_opened) return false;
  JsonDocument doc;
  loadDocument(doc);
  doc.remove(key);
  return saveDocument(doc);
}

bool Rp2040ArduinoStorage::hasWifi() const {
  std::string ssid;
  if (!rawGetString("wifi_ssid", ssid)) return false;
  return !ssid.empty();
}

bool Rp2040ArduinoStorage::loadWifiSsid(std::string& outSsid) const { return rawGetString("wifi_ssid", outSsid); }

bool Rp2040ArduinoStorage::saveWifiSsid(const std::string& ssid) {
  if (ssid.empty()) return false;
  return rawPutString("wifi_ssid", ssid);
}

bool Rp2040ArduinoStorage::clearWifi() { return rawRemove("wifi_ssid"); }

bool Rp2040ArduinoStorage::loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const {
  if (!_opened) return false;
  JsonDocument doc;
  loadDocument(doc);
  outIdentity.deviceId = std::string((const char*)(doc["device_id"] | ""));
  outIdentity.deviceTemplateId = std::string((const char*)(doc["device_template_id"] | ""));
  outIdentity.deviceTemplateVersion = doc["device_template_ver"] | 1;
  outIdentity.mqttClientId = std::string((const char*)(doc["mqtt_cid"] | ""));
  outIdentity.mqttUsername = std::string((const char*)(doc["mqtt_user"] | ""));
  outIdentity.mqttPassword = std::string((const char*)(doc["mqtt_pass"] | ""));
  outIdentity.mqttBroker = std::string((const char*)(doc["mqtt_host"] | ""));
  outIdentity.mqttPort = doc["mqtt_port"] | 8883;
  outIdentity.topicPrefix = std::string((const char*)(doc["topic_pref"] | ""));
  return true;
}

bool Rp2040ArduinoStorage::saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) {
  if (!_opened || !identity.valid()) return false;

  JsonDocument doc;
  loadDocument(doc);  // Preserve wifi_ssid/wifi_pass/act_token already in the same file.
  doc["device_id"] = identity.deviceId.c_str();
  doc["device_template_id"] = identity.deviceTemplateId.c_str();
  doc["device_template_ver"] = identity.deviceTemplateVersion;
  doc["mqtt_cid"] = identity.mqttClientId.c_str();
  doc["mqtt_user"] = identity.mqttUsername.c_str();
  doc["mqtt_pass"] = identity.mqttPassword.c_str();
  doc["mqtt_host"] = identity.mqttBroker.c_str();
  doc["mqtt_port"] = identity.mqttPort;
  doc["topic_pref"] = identity.topicPrefix.c_str();
  return saveDocument(doc);
}

bool Rp2040ArduinoStorage::clearRuntimeIdentity() {
  if (!_opened) return false;
  JsonDocument doc;
  loadDocument(doc);
  const char* keys[] = {"device_id", "profile_id", "profile_ver", "mqtt_cid", "mqtt_user",
                         "mqtt_pass", "mqtt_host",  "mqtt_port",   "topic_pref"};
  for (const char* key : keys) doc.remove(key);
  return saveDocument(doc);
}

bool Rp2040ArduinoStorage::clearAll() {
  if (!_opened) return false;
  const std::string path = filePath();
  if (!LittleFS.exists(path.c_str())) return true;
  return LittleFS.remove(path.c_str());
}

#endif  // ARDUINO_ARCH_RP2040
