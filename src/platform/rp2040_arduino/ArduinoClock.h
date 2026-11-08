#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include "../../core/ports/IClock.h"

class ArduinoClock : public IClock {
 public:
  uint32_t monotonicMillis() const override;
  void blockingDelayMs(uint32_t ms) const override;
  void restartDevice() override;
};

#endif  // ARDUINO_ARCH_RP2040
