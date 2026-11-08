#pragma once

#include "../../core/ports/IClock.h"

// Identical contract to the ESP32 adapter's ArduinoClock: millis()/delay()
// and a restart hook are both plain Arduino-core surface, portable
// verbatim between ESP32 and ESP8266 (only the .cpp's restart header
// differs — see ArduinoClock.cpp).
class ArduinoClock : public IClock {
 public:
  uint32_t monotonicMillis() const override;
  void blockingDelayMs(uint32_t ms) const override;
  void restartDevice() override;
};
