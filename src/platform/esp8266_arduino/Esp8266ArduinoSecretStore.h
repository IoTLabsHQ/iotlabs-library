#pragma once

#include "../../core/ports/ISecretStore.h"
#include "Esp8266ArduinoStorage.h"

// Composes over Esp8266ArduinoStorage's LittleFS-backed JSON store instead
// of opening a second file — same "one storage instance, two ports" shape
// as Esp32ArduinoSecretStore over Esp32ArduinoStorage's Preferences handle.
class Esp8266ArduinoSecretStore : public ISecretStore {
 public:
  explicit Esp8266ArduinoSecretStore(Esp8266ArduinoStorage& storage) : _storage(storage) {}

  bool loadWifiPassword(std::string& outPassword) const override;
  bool saveWifiPassword(const std::string& password) override;
  bool clearWifiPassword() override;

  bool loadActivationToken(std::string& outToken) const override;
  bool saveActivationToken(const std::string& token) override;
  bool clearActivationToken() override;

 private:
  Esp8266ArduinoStorage& _storage;
};
