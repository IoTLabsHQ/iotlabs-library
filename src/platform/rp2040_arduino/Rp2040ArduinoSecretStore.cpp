#include "Rp2040ArduinoSecretStore.h"

// No RP2040/arduino-pico headers used directly here (pure logic over
// Rp2040ArduinoStorage), but guarded anyway for consistency with every other
// .cpp in this platform directory — see ArduinoClock.cpp for why the guard
// is needed at all.
#if defined(ARDUINO_ARCH_RP2040)

bool Rp2040ArduinoSecretStore::loadWifiPassword(std::string& outPassword) const {
  return _storage.rawGetString("wifi_pass", outPassword);
}

bool Rp2040ArduinoSecretStore::saveWifiPassword(const std::string& password) {
  return _storage.rawPutString("wifi_pass", password);
}

bool Rp2040ArduinoSecretStore::clearWifiPassword() { return _storage.rawRemove("wifi_pass"); }

bool Rp2040ArduinoSecretStore::loadActivationToken(std::string& outToken) const {
  return _storage.rawGetString("act_token", outToken);
}

bool Rp2040ArduinoSecretStore::saveActivationToken(const std::string& token) {
  if (token.empty()) return false;
  return _storage.rawPutString("act_token", token);
}

bool Rp2040ArduinoSecretStore::clearActivationToken() { return _storage.rawRemove("act_token"); }

#endif  // ARDUINO_ARCH_RP2040
