#pragma once

#if !defined(ARDUINO)

#include <map>
#include <string>

#include "../../core/ports/IStorage.h"

// Filesystem-backed IStorage: a single JSON file (ArduinoJson -- the same
// ADR-006-accepted portable dependency src/core/ already uses, see
// IoTLabs internal design notes for how it's vendored natively)
// under a configurable state directory, holding the same flat key set the
// ESP32 adapter keeps in one NVS/Preferences namespace (see
// Esp32ArduinoStorage.cpp's rawGetString/rawPutString/rawRemove contract,
// mirrored here). Every write rewrites the whole file via a temp-file +
// rename() so a crash mid-write can never leave a half-written file.
//
// The file (and its directory) are created/chmod'd 0600/0700 because
// LinuxSecretStore composes on top of this same handle for wifi_pass and
// act_token -- see LinuxSecretStore.h for why that is "permission-
// restricted", not encrypted-at-rest (known limitation, documented in
// BOARD_QUALIFICATION.md).
class LinuxStorage : public IStorage {
 public:
  // stateDir: overrides the default state directory. Empty (default) picks
  // defaultStateDir() the first time begin() is called.
  explicit LinuxStorage(std::string stateDir = std::string());

  bool begin(const char* ns) override;

  bool hasWifi() const override;
  bool loadWifiSsid(std::string& outSsid) const override;
  bool saveWifiSsid(const std::string& ssid) override;
  bool clearWifi() override;

  bool loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const override;
  bool saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) override;
  bool clearRuntimeIdentity() override;

  bool clearAll() override;

  // Exposed for LinuxSecretStore composition only (mirrors
  // Esp32ArduinoStorage's rawGetString/rawPutString/rawRemove contract).
  bool rawGetString(const char* key, std::string& out) const;
  bool rawPutString(const char* key, const std::string& value);
  bool rawRemove(const char* key);

  // $HOME/.local/state/iotlabs (XDG "state dir" convention, without pulling
  // in a full XDG library -- deliberately not full XDG-spec compliant, see
  // BOARD_QUALIFICATION.md) when running as a normal user, or
  // /var/lib/iotlabs when running as root (uid 0) -- a reasonable default
  // for a system service. Not overengineered further for this candidate.
  static std::string defaultStateDir();

  // Full path to the backing file for the given namespace, for tests.
  std::string filePathForTest() const { return _filePath; }

 private:
  std::string _stateDir;
  std::string _filePath;
  std::map<std::string, std::string> _kv;
  bool _opened = false;

  bool loadFromDisk();
  bool writeToDisk() const;
};

#endif  // !ARDUINO
