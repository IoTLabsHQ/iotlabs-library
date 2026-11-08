#include "ArduinoLogger.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>

void ArduinoLogger::log(const char* line) { Serial.println(line); }

#endif  // ARDUINO_ARCH_ESP32
