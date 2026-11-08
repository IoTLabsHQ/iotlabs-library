#include "LinuxStorage.h"

#if !defined(ARDUINO)

#include <sys/stat.h>
#include <unistd.h>

#include <ArduinoJson.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

bool mkdirRecursive(const std::string& path, ::mode_t mode) {
  if (path.empty()) return false;
  std::string partial;
  size_t pos = 0;
  // Skip a leading '/', build up one path segment at a time.
  if (path[0] == '/') {
    partial = "/";
    pos = 1;
  }
  while (pos <= path.size()) {
    size_t next = path.find('/', pos);
    if (next == std::string::npos) next = path.size();
    partial += path.substr(pos, next - pos);
    if (!partial.empty()) {
      if (::mkdir(partial.c_str(), mode) != 0 && errno != EEXIST) return false;
    }
    partial += "/";
    pos = next + 1;
  }
  return true;
}

// Numeric fields are stored as decimal strings in the same flat map as
// everything else -- avoids a second ArduinoJson number-vs-string codepath
// for two fields (profile_ver, mqtt_port) and keeps rawGetString/
// rawPutString a single simple contract, same spirit as the ESP32 adapter's
// Preferences calls (which are already per-key-typed under the hood).
uint32_t parseU32(const std::string& s, uint32_t fallback) {
  if (s.empty()) return fallback;
  return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
}

}  // namespace

LinuxStorage::LinuxStorage(std::string stateDir) : _stateDir(std::move(stateDir)) {}

std::string LinuxStorage::defaultStateDir() {
  if (::geteuid() == 0) return "/var/lib/iotlabs";
  const char* home = std::getenv("HOME");
  if (home && home[0] != '\0') return std::string(home) + "/.local/state/iotlabs";
  return "/var/lib/iotlabs";
}

bool LinuxStorage::begin(const char* ns) {
  if (_opened) return true;
  if (!ns || ns[0] == '\0') return false;

  if (_stateDir.empty()) _stateDir = defaultStateDir();
  if (!mkdirRecursive(_stateDir, 0700)) return false;
  ::chmod(_stateDir.c_str(), 0700);

  _filePath = _stateDir + "/" + ns + ".json";
  loadFromDisk();  // Missing file is fine -- fresh device, empty map.
  _opened = true;
  return true;
}

bool LinuxStorage::loadFromDisk() {
  std::ifstream file(_filePath);
  if (!file.is_open()) return false;
  std::ostringstream contents;
  contents << file.rdbuf();
  file.close();

  JsonDocument doc;
  if (deserializeJson(doc, contents.str()) != DeserializationError::Ok) return false;
  if (!doc.is<JsonObject>()) return false;

  _kv.clear();
  for (JsonPair kv : doc.as<JsonObject>()) {
    _kv[kv.key().c_str()] = kv.value().as<std::string>();
  }
  return true;
}

bool LinuxStorage::writeToDisk() const {
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  for (const auto& entry : _kv) obj[entry.first] = entry.second;

  const std::string tmpPath = _filePath + ".tmp";
  {
    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out.is_open()) return false;
    serializeJson(doc, out);
    if (!out.good()) return false;
  }
  ::chmod(tmpPath.c_str(), 0600);

  // Atomic on POSIX when tmp and target share a filesystem (same directory
  // here), so a crash mid-write can never leave a half-written state file.
  if (::rename(tmpPath.c_str(), _filePath.c_str()) != 0) {
    ::unlink(tmpPath.c_str());
    return false;
  }
  ::chmod(_filePath.c_str(), 0600);
  return true;
}

bool LinuxStorage::rawGetString(const char* key, std::string& out) const {
  if (!_opened || !key) return false;
  auto it = _kv.find(key);
  if (it == _kv.end()) return false;
  out = it->second;
  return true;
}

bool LinuxStorage::rawPutString(const char* key, const std::string& value) {
  if (!_opened || !key) return false;
  _kv[key] = value;
  return writeToDisk();
}

bool LinuxStorage::rawRemove(const char* key) {
  if (!_opened || !key) return false;
  _kv.erase(key);
  return writeToDisk();
}

bool LinuxStorage::hasWifi() const {
  auto it = _kv.find("wifi_ssid");
  return _opened && it != _kv.end() && !it->second.empty();
}

bool LinuxStorage::loadWifiSsid(std::string& outSsid) const { return rawGetString("wifi_ssid", outSsid); }

bool LinuxStorage::saveWifiSsid(const std::string& ssid) {
  if (ssid.empty()) return false;
  return rawPutString("wifi_ssid", ssid);
}

bool LinuxStorage::clearWifi() { return rawRemove("wifi_ssid"); }

bool LinuxStorage::loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const {
  if (!_opened) return false;
  auto get = [this](const char* key) -> std::string {
    auto it = _kv.find(key);
    return it == _kv.end() ? std::string() : it->second;
  };
  outIdentity.deviceId = get("device_id");
  outIdentity.deviceTemplateId = get("device_template_id");
  outIdentity.deviceTemplateVersion = parseU32(get("device_template_ver"), 1);
  outIdentity.mqttClientId = get("mqtt_cid");
  outIdentity.mqttUsername = get("mqtt_user");
  outIdentity.mqttPassword = get("mqtt_pass");
  outIdentity.mqttBroker = get("mqtt_host");
  outIdentity.mqttPort = static_cast<uint16_t>(parseU32(get("mqtt_port"), 8883));
  outIdentity.topicPrefix = get("topic_pref");
  return true;
}

bool LinuxStorage::saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) {
  if (!_opened || !identity.valid()) return false;

  _kv["device_id"] = identity.deviceId;
  _kv["device_template_id"] = identity.deviceTemplateId;
  _kv["device_template_ver"] = std::to_string(identity.deviceTemplateVersion);
  _kv["mqtt_cid"] = identity.mqttClientId;
  _kv["mqtt_user"] = identity.mqttUsername;
  _kv["mqtt_pass"] = identity.mqttPassword;
  _kv["mqtt_host"] = identity.mqttBroker;
  _kv["mqtt_port"] = std::to_string(identity.mqttPort);
  _kv["topic_pref"] = identity.topicPrefix;
  return writeToDisk();
}

bool LinuxStorage::clearRuntimeIdentity() {
  if (!_opened) return false;
  static const char* keys[] = {"device_id", "profile_id", "profile_ver", "mqtt_cid", "mqtt_user",
                                "mqtt_pass", "mqtt_host",  "mqtt_port",   "topic_pref"};
  for (const char* key : keys) _kv.erase(key);
  return writeToDisk();
}

bool LinuxStorage::clearAll() {
  if (!_opened) return false;
  _kv.clear();
  return writeToDisk();
}

#endif  // !ARDUINO
