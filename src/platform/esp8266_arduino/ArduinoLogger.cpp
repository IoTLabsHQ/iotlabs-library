#include "ArduinoLogger.h"

// Same rationale as ArduinoClock.cpp: guard against being compiled (and
// linked) for a non-ESP8266 board, and against colliding with
// src/platform/esp32_arduino/ArduinoLogger.cpp's identically-named class.
#if defined(ARDUINO_ARCH_ESP8266)

#include <Arduino.h>

void ArduinoLogger::log(const char* line) { Serial.println(line); }

#endif  // ARDUINO_ARCH_ESP8266
