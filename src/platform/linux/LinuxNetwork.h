#pragma once

#if !defined(ARDUINO)

#include "../../core/ports/INetwork.h"

// "Host connectivity state", not WiFi radio management (see
// IoTLabs internal design notes) -- Linux apps don't
// normally own WiFi credentials the way an ESP32 sketch owns WiFi.begin();
// NetworkManager/wpa_supplicant/systemd-networkd do that job on a real
// Linux/RPi host. connect()/beginStation() are therefore no-ops that assume
// the OS already manages connectivity (each logs what it is/isn't doing).
// stationConnected() is a REAL check (iterates getifaddrs() for a non-
// loopback UP+RUNNING interface), not a hardcoded true -- a "candidate"
// adapter that always reports connected regardless of reality would defeat
// the whole point of implementing the port. scanNetworks() is unsupported
// on this adapter (capability-based support is explicit policy, see
// 00_PLATFORM_SUPPORT_POLICY.md: "a platform may support Core + MQTT
// without supporting SoftAP provisioning").
class LinuxNetwork : public INetwork {
 public:
  void beginStation() override;
  bool connect(const char* ssid, const char* password) override;
  void disconnect(bool eraseCredentials) override;
  bool stationConnected() const override;

  // Always returns 0 -- see class comment. Linux WiFi scanning needs a
  // specific stack (iw/nl80211 or NetworkManager D-Bus) that is out of
  // scope for this candidate adapter.
  int scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) override;
};

#endif  // !ARDUINO
