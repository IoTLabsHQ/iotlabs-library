#pragma once

#include <cstddef>

// Stable per-chip identifier used for device claiming/activation. Adapters
// derive this from whatever unique silicon ID the platform exposes.
class IHardwareIdentity {
 public:
  virtual ~IHardwareIdentity() = default;

  // Writes a NUL-terminated hex id into buffer (caller-owned, bufferSize includes NUL).
  virtual void readHardwareId(char* buffer, size_t bufferSize) const = 0;
};
