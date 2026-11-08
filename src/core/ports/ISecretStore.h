#pragma once

#include <string>

// Credential-bearing persistence that stands apart from IStorage's runtime
// identity record: the Wi-Fi password and the one-time activation token.
// (MQTT username/password travel as part of IoTLabsRuntimeIdentity, saved
// atomically through IStorage::saveRuntimeIdentity — splitting that struct
// across two ports mid-flight would buy nothing for phase 1.) Kept separate
// from IStorage so a future secure-element/encrypted-partition adapter can
// back only this port (RT-09 rotation).
class ISecretStore {
 public:
  virtual ~ISecretStore() = default;

  virtual bool loadWifiPassword(std::string& outPassword) const = 0;
  virtual bool saveWifiPassword(const std::string& password) = 0;
  virtual bool clearWifiPassword() = 0;

  virtual bool loadActivationToken(std::string& outToken) const = 0;
  virtual bool saveActivationToken(const std::string& token) = 0;
  virtual bool clearActivationToken() = 0;
};
