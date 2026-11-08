#include "Esp8266ArduinoSecretStore.h"

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board; guarded for consistency with the rest of this platform
// directory even though this particular file has no direct ESP8266-only
// include (it only forwards to Esp8266ArduinoStorage's raw ops).
#if defined(ARDUINO_ARCH_ESP8266)

bool Esp8266ArduinoSecretStore::loadWifiPassword(std::string& outPassword) const {
  return _storage.rawGetString("wifi_pass", outPassword);
}

bool Esp8266ArduinoSecretStore::saveWifiPassword(const std::string& password) {
  return _storage.rawPutString("wifi_pass", password);
}

bool Esp8266ArduinoSecretStore::clearWifiPassword() { return _storage.rawRemove("wifi_pass"); }

bool Esp8266ArduinoSecretStore::loadActivationToken(std::string& outToken) const {
  return _storage.rawGetString("act_token", outToken);
}

bool Esp8266ArduinoSecretStore::saveActivationToken(const std::string& token) {
  if (token.empty()) return false;
  return _storage.rawPutString("act_token", token);
}

bool Esp8266ArduinoSecretStore::clearActivationToken() { return _storage.rawRemove("act_token"); }

#endif  // ARDUINO_ARCH_ESP8266
