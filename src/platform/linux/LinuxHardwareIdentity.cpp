#include "LinuxHardwareIdentity.h"

#if !defined(ARDUINO)

#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

// FNV-1a 64-bit: small, dependency-free, and more than sufficient for
// "stable per-process identifier for activation", not a cryptographic
// requirement (same non-goal as ESP32's raw eFuse MAC readout).
uint64_t fnv1a64(const std::string& data) {
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : data) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool readMachineId(std::string& out) {
  std::ifstream file("/etc/machine-id");
  if (!file.is_open()) return false;
  std::ostringstream contents;
  contents << file.rdbuf();
  out = contents.str();
  // Trim trailing whitespace/newline systemd writes.
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
  return !out.empty();
}

std::string hostnameFallback() {
  char host[256] = {0};
  if (gethostname(host, sizeof(host) - 1) != 0) return std::string("unknown-host");
  return std::string(host);
}

}  // namespace

void LinuxHardwareIdentity::readHardwareId(char* buffer, size_t bufferSize) const {
  if (!buffer || bufferSize == 0) return;

  std::string source;
  bool usedMachineId = readMachineId(source);
  if (!usedMachineId) {
    // KNOWN LIMITATION (documented in BOARD_QUALIFICATION.md too): the
    // hostname fallback is materially weaker than /etc/machine-id --
    // hostnames are frequently reused/defaulted across machines (e.g.
    // "localhost", "raspberrypi") and are user-changeable, so this fallback
    // does not provide the same per-chip uniqueness guarantee the ESP32
    // eFuse MAC or a real /etc/machine-id does. It exists so the adapter
    // still returns *something* deterministic rather than failing outright.
    source = hostnameFallback();
  }

  const uint64_t hash = fnv1a64(source);
  std::snprintf(buffer, bufferSize, "%016llX", static_cast<unsigned long long>(hash));
}

#endif  // !ARDUINO
