#include "LinuxNetwork.h"

#if !defined(ARDUINO)

#include <ifaddrs.h>
#include <net/if.h>

#include <cstdio>
#include <cstring>
#include <string>

void LinuxNetwork::beginStation() {
  std::fprintf(stderr,
               "[LinuxNetwork] beginStation(): no-op on this adapter -- assuming the OS "
               "(NetworkManager/wpa_supplicant/systemd-networkd) already manages station "
               "connectivity. stationConnected() reports the real interface state.\n");
}

bool LinuxNetwork::connect(const char* ssid, const char* password) {
  (void)password;
  std::fprintf(stderr,
               "[LinuxNetwork] connect(ssid=\"%s\") ignored: this adapter does not manage WiFi "
               "credentials -- point the host's own network manager at this SSID out of band. "
               "Returning true to let the engine proceed to polling stationConnected().\n",
               ssid ? ssid : "");
  return true;
}

void LinuxNetwork::disconnect(bool eraseCredentials) {
  (void)eraseCredentials;
  std::fprintf(stderr, "[LinuxNetwork] disconnect(): no-op -- this adapter does not own the OS connection.\n");
}

bool LinuxNetwork::stationConnected() const {
  struct ifaddrs* addrs = nullptr;
  if (getifaddrs(&addrs) != 0) return false;

  bool up = false;
  for (struct ifaddrs* it = addrs; it != nullptr; it = it->ifa_next) {
    if (!it->ifa_name || !it->ifa_flags) continue;
    if (std::string(it->ifa_name) == "lo") continue;
    const unsigned int required = IFF_UP | IFF_RUNNING;
    if ((it->ifa_flags & required) == required) {
      up = true;
      break;
    }
  }
  freeifaddrs(addrs);
  return up;
}

int LinuxNetwork::scanNetworks(IoTLabsNetworkScanResult* results, int maxResults) {
  (void)results;
  (void)maxResults;
  std::fprintf(stderr,
               "[LinuxNetwork] scanNetworks(): not supported by this adapter (needs iw/nl80211 or "
               "NetworkManager D-Bus, out of scope). Returning 0 -- see 00_PLATFORM_SUPPORT_POLICY.md.\n");
  return 0;
}

#endif  // !ARDUINO
