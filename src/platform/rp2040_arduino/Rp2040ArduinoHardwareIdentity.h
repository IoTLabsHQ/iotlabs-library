#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include "../../core/ports/IHardwareIdentity.h"

class Rp2040ArduinoHardwareIdentity : public IHardwareIdentity {
 public:
  void readHardwareId(char* buffer, size_t bufferSize) const override;
};

#endif  // ARDUINO_ARCH_RP2040
