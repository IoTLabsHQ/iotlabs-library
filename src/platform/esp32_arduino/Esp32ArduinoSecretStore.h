#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "../../core/ports/ISecretStore.h"
#include "Esp32ArduinoStorage.h"

// Composes over Esp32ArduinoStorage's already-open Preferences handle
// instead of opening a second one on the same NVS namespace.
class Esp32ArduinoSecretStore : public ISecretStore {
 public:
  explicit Esp32ArduinoSecretStore(Esp32ArduinoStorage& storage) : _storage(storage) {}

  bool loadWifiPassword(std::string& outPassword) const override;
  bool saveWifiPassword(const std::string& password) override;
  bool clearWifiPassword() override;

  bool loadActivationToken(std::string& outToken) const override;
  bool saveActivationToken(const std::string& token) override;
  bool clearActivationToken() override;

 private:
  Esp32ArduinoStorage& _storage;
};

#endif  // ARDUINO_ARCH_ESP32
