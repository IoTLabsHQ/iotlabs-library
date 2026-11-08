#pragma once

#include "../../core/ports/ILogger.h"

// Identical to the ESP32 adapter's ArduinoLogger: Serial.println is the
// same call on both cores. Kept as its own tiny class (rather than shared
// source) so this platform directory stays self-contained per
// src/platform/<platform>/ (ADR-006 consequence #2).
class ArduinoLogger : public ILogger {
 public:
  void log(const char* line) override;
};
