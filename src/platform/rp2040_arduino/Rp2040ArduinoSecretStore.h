#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include "../../core/ports/ISecretStore.h"
#include "Rp2040ArduinoStorage.h"

// Composes over Rp2040ArduinoStorage's already-open LittleFS-backed key/value
// file instead of opening a second one — same pattern as
// Esp32ArduinoSecretStore over Esp32ArduinoStorage's Preferences handle.
class Rp2040ArduinoSecretStore : public ISecretStore {
 public:
  explicit Rp2040ArduinoSecretStore(Rp2040ArduinoStorage& storage) : _storage(storage) {}

  bool loadWifiPassword(std::string& outPassword) const override;
  bool saveWifiPassword(const std::string& password) override;
  bool clearWifiPassword() override;

  bool loadActivationToken(std::string& outToken) const override;
  bool saveActivationToken(const std::string& token) override;
  bool clearActivationToken() override;

 private:
  Rp2040ArduinoStorage& _storage;
};

#endif  // ARDUINO_ARCH_RP2040
