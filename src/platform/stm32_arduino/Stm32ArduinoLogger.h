#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include "../../core/ports/ILogger.h"

class Stm32ArduinoLogger : public ILogger {
 public:
  void log(const char* line) override;
};

#endif  // ARDUINO_ARCH_STM32
