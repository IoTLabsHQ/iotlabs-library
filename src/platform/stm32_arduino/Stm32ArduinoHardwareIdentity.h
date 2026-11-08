#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include "../../core/ports/IHardwareIdentity.h"

class Stm32ArduinoHardwareIdentity : public IHardwareIdentity {
 public:
  void readHardwareId(char* buffer, size_t bufferSize) const override;
};

#endif  // ARDUINO_ARCH_STM32
