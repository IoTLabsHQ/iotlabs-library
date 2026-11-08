#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include "../../core/ports/ISecretStore.h"
#include "Stm32ArduinoStorage.h"

// Composes over Stm32ArduinoStorage's already-initialized EEPROM-emulated
// flash page instead of maintaining a second independent record, exactly
// like Esp32ArduinoSecretStore composes over Esp32ArduinoStorage's open
// Preferences handle.
class Stm32ArduinoSecretStore : public ISecretStore {
 public:
  explicit Stm32ArduinoSecretStore(Stm32ArduinoStorage& storage) : _storage(storage) {}

  bool loadWifiPassword(std::string& outPassword) const override;
  bool saveWifiPassword(const std::string& password) override;
  bool clearWifiPassword() override;

  bool loadActivationToken(std::string& outToken) const override;
  bool saveActivationToken(const std::string& token) override;
  bool clearActivationToken() override;

 private:
  Stm32ArduinoStorage& _storage;
};

#endif  // ARDUINO_ARCH_STM32
