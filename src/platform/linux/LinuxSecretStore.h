#pragma once

#if !defined(ARDUINO)

#include "../../core/ports/ISecretStore.h"
#include "LinuxStorage.h"

// Composes over LinuxStorage's already-open backing file instead of a
// second one -- same pattern as Esp32ArduinoSecretStore over
// Esp32ArduinoStorage's single Preferences namespace.
//
// ISecretStore's contract asks for "permission-restricted/encrypted store
// as available" (05_LINUX_RASPBERRY_PI.md). This candidate adapter only
// does the permission-restricted half: LinuxStorage already chmod's its
// backing file 0600 and its directory 0700 on every write, which is the
// enforcement this class relies on. It does NOT encrypt wifi_pass/
// act_token at rest -- a real OS keychain/libsecret integration is out of
// scope here and is called out as a known limitation in
// BOARD_QUALIFICATION.md for the "Stable" bar later.
class LinuxSecretStore : public ISecretStore {
 public:
  explicit LinuxSecretStore(LinuxStorage& storage) : _storage(storage) {}

  bool loadWifiPassword(std::string& outPassword) const override;
  bool saveWifiPassword(const std::string& password) override;
  bool clearWifiPassword() override;

  bool loadActivationToken(std::string& outToken) const override;
  bool saveActivationToken(const std::string& token) override;
  bool clearActivationToken() override;

 private:
  LinuxStorage& _storage;
};

#endif  // !ARDUINO
