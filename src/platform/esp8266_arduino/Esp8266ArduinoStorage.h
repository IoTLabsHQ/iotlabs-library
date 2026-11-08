#pragma once

#include <ArduinoJson.h>

#include <string>

#include "../../core/ports/IStorage.h"

// ESP8266 has no Preferences.h (that API is ESP32/NVS-only). This adapter
// backs IStorage with a single LittleFS-resident JSON file instead (built
// into the ESP8266 core; ArduinoJson is an already-accepted portable
// dependency per ADR-006). One open LittleFS volume backs both this port
// and Esp8266ArduinoSecretStore (composed on top of it), mirroring
// Esp32ArduinoStorage's single-namespace shape.
//
// Document is loaded/saved per operation rather than kept resident in RAM,
// trading a little flash I/O per call for a bounded memory footprint —
// deliberate given 02_ESP8266_ARDUINO.md's "significantly tighter RAM"
// risk note.
class Esp8266ArduinoStorage : public IStorage {
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

  // Exposed for Esp8266ArduinoSecretStore composition only.
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
