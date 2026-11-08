#pragma once

#include <string>

#include "../IoTLabsRuntimeIdentity.h"

// Non-secret device configuration persistence (Wi-Fi SSID, runtime identity
// fields). See ISecretStore for the credential-bearing counterpart.
class IStorage {
 public:
  virtual ~IStorage() = default;

  virtual bool begin(const char* ns) = 0;

  virtual bool hasWifi() const = 0;
  virtual bool loadWifiSsid(std::string& outSsid) const = 0;
  virtual bool saveWifiSsid(const std::string& ssid) = 0;
  virtual bool clearWifi() = 0;

  virtual bool loadRuntimeIdentity(IoTLabsRuntimeIdentity& outIdentity) const = 0;
  virtual bool saveRuntimeIdentity(const IoTLabsRuntimeIdentity& identity) = 0;
  virtual bool clearRuntimeIdentity() = 0;

  // AutomationEngine's persisted rule set, stored as a single opaque
  // serialized-JSON blob (the array synced down from Device Platform).
  // Non-pure with a safe no-op default — same Interface Segregation pattern
  // as IProvisioningTransport::setProvisioningPageTitle(): platforms that
  // don't yet implement local rule persistence (esp8266/rp2040/stm32/linux)
  // compile unchanged and simply don't persist rules across reboot; only
  // Esp32ArduinoStorage overrides these for now.
  virtual bool saveAutomationRules(const std::string& /*json*/) { return false; }
  virtual bool loadAutomationRules(std::string& /*outJson*/) const { return false; }
  virtual bool clearAutomationRules() { return false; }

  virtual bool clearAll() = 0;
};
