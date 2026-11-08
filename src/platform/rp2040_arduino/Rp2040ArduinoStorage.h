#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include <ArduinoJson.h>

#include <string>

#include "../../core/ports/IStorage.h"

// LittleFS-backed key/value store: one JSON file (see kFileName in the .cpp)
// holding every effective key this port and Rp2040ArduinoSecretStore (composed
// on top of it) need, mirroring Esp32ArduinoStorage's single-Preferences-
// namespace shape but adapted to arduino-pico's flash filesystem instead of
// NVS. One open LittleFS volume backs both ports.
class Rp2040ArduinoStorage : public IStorage {
 public:
  bool begin(const char* ns) override;

  bool hasWifi() const override;
  bool loadWifiSsid(std::string& outSsid) const override;
  bool saveWifiSsid(const std::string& ssid) override;
  bool clearWifi() override;

  bool loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const override;
  bool saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) override;
  bool clearRuntimeIdentity() override;

  bool clearAll() override;

  // Exposed for Rp2040ArduinoSecretStore composition only.
  bool rawGetString(const char* key, std::string& out) const;
  bool rawPutString(const char* key, const std::string& value);
  bool rawRemove(const char* key);

 private:
  bool _opened = false;
  std::string _ns;

  std::string filePath() const;
  bool loadDocument(JsonDocument& doc) const;
  bool saveDocument(const JsonDocument& doc);
};

#endif  // ARDUINO_ARCH_RP2040
