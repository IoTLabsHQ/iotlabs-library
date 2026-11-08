#include "Esp32ArduinoSecretStore.h"

#if defined(ARDUINO_ARCH_ESP32)

bool Esp32ArduinoSecretStore::loadWifiPassword(std::string& outPassword) const {
  return _storage.rawGetString("wifi_pass", outPassword);
}

bool Esp32ArduinoSecretStore::saveWifiPassword(const std::string& password) {
  return _storage.rawPutString("wifi_pass", password);
}

bool Esp32ArduinoSecretStore::clearWifiPassword() { return _storage.rawRemove("wifi_pass"); }

bool Esp32ArduinoSecretStore::loadActivationToken(std::string& outToken) const {
  return _storage.rawGetString("act_token", outToken);
}

bool Esp32ArduinoSecretStore::saveActivationToken(const std::string& token) {
  if (token.empty()) return false;
  return _storage.rawPutString("act_token", token);
}

bool Esp32ArduinoSecretStore::clearActivationToken() { return _storage.rawRemove("act_token"); }

#endif  // ARDUINO_ARCH_ESP32
