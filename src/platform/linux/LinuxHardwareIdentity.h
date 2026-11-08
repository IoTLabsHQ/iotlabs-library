#pragma once

#if !defined(ARDUINO)

#include "../../core/ports/IHardwareIdentity.h"

// Reads /etc/machine-id (systemd standard; present on Debian/Raspberry Pi OS
// and most modern distros) and hashes/truncates it to a stable hex string.
// Falls back to gethostname() when /etc/machine-id is missing -- see
// LinuxHardwareIdentity.cpp for why that fallback has a weaker uniqueness
// guarantee (documented there rather than here, next to the code that does
// it).
class LinuxHardwareIdentity : public IHardwareIdentity {
 public:
  void readHardwareId(char* buffer, size_t bufferSize) const override;
};

#endif  // !ARDUINO
