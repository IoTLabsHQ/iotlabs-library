#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>

#include "../../core/ports/IStorage.h"

// Single NVS/Preferences namespace backs both this port and
// Esp32ArduinoSecretStore (composed on top of it) — one open handle, same
// as the original monolith's single `Preferences _prefs`.
class Esp32ArduinoStorage : public IStorage {
 public:
  bool begin(const char* ns) override;

  bool hasWifi() const override;
  bool loadWifiSsid(std::string& outSsid) const override;
  bool saveWifiSsid(const std::string& ssid) override;
  bool clearWifi() override;

  bool loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const override;
  bool saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) override;
  bool clearRuntimeIdentity() override;

  bool saveAutomationRules(const std::string& json) override;
  bool loadAutomationRules(std::string& outJson) const override;
  bool clearAutomationRules() override;

  bool clearAll() override;

  // Exposed for Esp32ArduinoSecretStore composition only.
  bool rawGetString(const char* key, std::string& out) const;
  bool rawPutString(const char* key, const std::string& value);
  bool rawRemove(const char* key);

 private:
  mutable Preferences _prefs;
  bool _opened = false;
};

#endif  // ARDUINO_ARCH_ESP32
