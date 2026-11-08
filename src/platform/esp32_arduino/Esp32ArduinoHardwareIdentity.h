#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "../../core/ports/IHardwareIdentity.h"

class Esp32ArduinoHardwareIdentity : public IHardwareIdentity {
 public:
  void readHardwareId(char* buffer, size_t bufferSize) const override;
};

#endif  // ARDUINO_ARCH_ESP32
