#include "LinuxSecretStore.h"

#if !defined(ARDUINO)

bool LinuxSecretStore::loadWifiPassword(std::string& outPassword) const {
  return _storage.rawGetString("wifi_pass", outPassword);
}

bool LinuxSecretStore::saveWifiPassword(const std::string& password) {
  return _storage.rawPutString("wifi_pass", password);
}

bool LinuxSecretStore::clearWifiPassword() { return _storage.rawRemove("wifi_pass"); }

bool LinuxSecretStore::loadActivationToken(std::string& outToken) const {
  return _storage.rawGetString("act_token", outToken);
}

bool LinuxSecretStore::saveActivationToken(const std::string& token) {
  if (token.empty()) return false;
  return _storage.rawPutString("act_token", token);
}

bool LinuxSecretStore::clearActivationToken() { return _storage.rawRemove("act_token"); }

#endif  // !ARDUINO
