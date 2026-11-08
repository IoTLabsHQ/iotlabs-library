#pragma once

#include <cstddef>
#include <cstdint>

struct IoTLabsNetworkScanResult {
  static constexpr size_t SSID_MAX = 33;
  char ssid[SSID_MAX] = {0};
  int32_t rssi = 0;
  bool secure = false;
};

// Station-side connectivity. Local-AP + HTTP lifecycle lives on
// IProvisioningTransport instead, since on every currently supported board
// SoftAP and the local provisioning server are one cohesive mechanism.
class INetwork {
 public:
  virtual ~INetwork() = default;

  virtual void beginStation() = 0;
  virtual bool connect(const char* ssid, const char* password) = 0;
  virtual void disconnect(bool eraseCredentials) = 0;
  virtual bool stationConnected() const = 0;

  // Returns network count written into results (<= maxResults), or -1 on failure.
  virtual int scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) = 0;
};
