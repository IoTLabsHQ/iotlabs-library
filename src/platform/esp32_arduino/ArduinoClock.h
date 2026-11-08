#pragma once

// Every .h/.cpp under src/platform/<x>/ self-guards to an empty translation
// unit on a mismatched arch: Arduino's build system compiles every .cpp
// under src/ for every sketch regardless of target board, so without this
// guard an ESP32 build would fail trying to pull in another platform's
// headers the moment a second platform directory exists.
#if defined(ARDUINO_ARCH_ESP32)

#include "../../core/ports/IClock.h"

class ArduinoClock : public IClock {
 public:
  uint32_t monotonicMillis() const override;
  void blockingDelayMs(uint32_t ms) const override;
  void restartDevice() override;
};

#endif  // ARDUINO_ARCH_ESP32
