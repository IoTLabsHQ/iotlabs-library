#include "Esp8266ArduinoStorage.h"

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board; <LittleFS.h> here is the ESP8266 core's copy (bundled
// separately per-core, API not guaranteed identical elsewhere), so this
// whole file no-ops outside an ESP8266 build.
#if defined(ARDUINO_ARCH_ESP8266)

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace {
// All effective keys this port and Esp8266ArduinoSecretStore need, kept in
// one flat JSON object per file — same key names as Esp32ArduinoStorage's
// Preferences keys, so a device migrating between adapters keeps its
// meaning even though the on-flash format differs.
constexpr const char* kRuntimeIdentityKeys[] = {"device_id", "profile_id", "profile_ver", "mqtt_cid",
                                                 "mqtt_user", "mqtt_pass",  "mqtt_host",   "mqtt_port",
                                                 "topic_pref"};
}  // namespace

std::string Esp8266ArduinoStorage::filePath() const { return "/" + _ns + ".json"; }

bool Esp8266ArduinoStorage::loadDocument(JsonDocument& doc) const {
  if (!_opened) return false;
  File f = LittleFS.open(filePath().c_str(), "r");
  if (!f) {
    doc.clear();
    return true;  // No file yet == empty store, not a failure.
  }
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) doc.clear();
  return true;
}

bool Esp8266ArduinoStorage::saveDocument(const JsonDocument& doc) {
  if (!_opened) return false;
  File f = LittleFS.open(filePath().c_str(), "w");
  if (!f) return false;
  const size_t written = serializeJson(doc, f);
  f.close();
  return written > 0;
}

bool Esp8266ArduinoStorage::begin(const char* ns) {
  if (_opened) return true;
  if (!ns || ns[0] == '\0') return false;
  if (!LittleFS.begin()) return false;
  _ns = ns;
  _opened = true;
  return true;
}

bool Esp8266ArduinoStorage::rawGetString(const char* key, std::string& out) const {
  if (!_opened) return false;
  JsonDocument doc;
  if (!loadDocument(doc)) return false;
  const char* value = doc[key] | "";
  out = value;
  return true;
}

bool Esp8266ArduinoStorage::rawPutString(const char* key, const std::string& value) {
  if (!_opened) return false;
  JsonDocument doc;
  if (!loadDocument(doc)) return false;
  doc[key] = value.c_str();
  return saveDocument(doc);
}

bool Esp8266ArduinoStorage::rawRemove(const char* key) {
  if (!_opened) return false;
  JsonDocument doc;
  if (!loadDocument(doc)) return false;
  doc.remove(key);
  return saveDocument(doc);
}

bool Esp8266ArduinoStorage::hasWifi() const {
  std::string ssid;
  return rawGetString("wifi_ssid", ssid) && !ssid.empty();
}

bool Esp8266ArduinoStorage::loadWifiSsid(std::string& outSsid) const { return rawGetString("wifi_ssid", outSsid); }

bool Esp8266ArduinoStorage::saveWifiSsid(const std::string& ssid) {
  if (ssid.empty()) return false;
  return rawPutString("wifi_ssid", ssid);
}

bool Esp8266ArduinoStorage::clearWifi() { return rawRemove("wifi_ssid"); }

bool Esp8266ArduinoStorage::loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const {
  if (!_opened) return false;
  JsonDocument doc;
  if (!loadDocument(doc)) return false;
  outIdentity.deviceId = std::string(doc["device_id"] | "");
  outIdentity.deviceTemplateId = std::string(doc["device_template_id"] | "");
  outIdentity.deviceTemplateVersion = doc["device_template_ver"] | 1;
  outIdentity.mqttClientId = std::string(doc["mqtt_cid"] | "");
  outIdentity.mqttUsername = std::string(doc["mqtt_user"] | "");
  outIdentity.mqttPassword = std::string(doc["mqtt_pass"] | "");
  outIdentity.mqttBroker = std::string(doc["mqtt_host"] | "");
  outIdentity.mqttPort = doc["mqtt_port"] | 8883;
  outIdentity.topicPrefix = std::string(doc["topic_pref"] | "");
  return true;
}

bool Esp8266ArduinoStorage::saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) {
  if (!_opened || !identity.valid()) return false;
  JsonDocument doc;
  if (!loadDocument(doc)) return false;
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

bool Esp8266ArduinoStorage::clearRuntimeIdentity() {
  if (!_opened) return false;
  JsonDocument doc;
  if (!loadDocument(doc)) return false;
  for (const char* key : kRuntimeIdentityKeys) doc.remove(key);
  return saveDocument(doc);
}

bool Esp8266ArduinoStorage::clearAll() {
  if (!_opened) return false;
  LittleFS.remove(filePath().c_str());
  JsonDocument empty;
  return saveDocument(empty);  // Re-initializes a fresh, truly-empty file.
}

#endif  // ARDUINO_ARCH_ESP8266
