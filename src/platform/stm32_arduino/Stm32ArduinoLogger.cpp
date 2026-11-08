#include "Stm32ArduinoLogger.h"

#if defined(ARDUINO_ARCH_STM32)

#include <Arduino.h>

// STM32duino's `Serial` on a Nucleo-144 board is the ST-Link virtual COM
// port (no USB CDC wiring needed) and behaves like every other Arduino
// core's Serial object, so this is identical to the ESP32 adapter's
// ArduinoLogger.
void Stm32ArduinoLogger::log(const char* line) { Serial.println(line); }

#endif  // ARDUINO_ARCH_STM32
