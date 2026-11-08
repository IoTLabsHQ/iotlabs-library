#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include "../../core/ports/ILogger.h"

class ArduinoLogger : public ILogger {
 public:
  void log(const char* line) override;
};

#endif  // ARDUINO_ARCH_RP2040
