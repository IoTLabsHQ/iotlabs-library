#include "Stm32ArduinoSecretStore.h"

#if defined(ARDUINO_ARCH_STM32)

bool Stm32ArduinoSecretStore::loadWifiPassword(std::string& outPassword) const {
  return _storage.rawReadField(Stm32ArduinoStorage::Field::kWifiPass, outPassword);
}

bool Stm32ArduinoSecretStore::saveWifiPassword(const std::string& password) {
  return _storage.rawWriteField(Stm32ArduinoStorage::Field::kWifiPass, password);
}

bool Stm32ArduinoSecretStore::clearWifiPassword() {
  return _storage.rawClearField(Stm32ArduinoStorage::Field::kWifiPass);
}

bool Stm32ArduinoSecretStore::loadActivationToken(std::string& outToken) const {
  return _storage.rawReadField(Stm32ArduinoStorage::Field::kActToken, outToken);
}

bool Stm32ArduinoSecretStore::saveActivationToken(const std::string& token) {
  if (token.empty()) return false;
  return _storage.rawWriteField(Stm32ArduinoStorage::Field::kActToken, token);
}

bool Stm32ArduinoSecretStore::clearActivationToken() {
  return _storage.rawClearField(Stm32ArduinoStorage::Field::kActToken);
}

#endif  // ARDUINO_ARCH_STM32
