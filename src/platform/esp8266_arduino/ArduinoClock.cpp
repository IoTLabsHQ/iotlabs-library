#include "ArduinoClock.h"

// Arduino's library build system compiles every .cpp under src/ for every
// sketch on every board, so this ESP8266-only translation unit must no-op
// when building for anything else -- both to avoid pulling in headers that
// don't exist on other cores, and to avoid a duplicate-symbol link error
// against src/platform/esp32_arduino/ArduinoClock.cpp, which defines the
// exact same (deliberately generic, unnamespaced) `ArduinoClock` class.
#if defined(ARDUINO_ARCH_ESP8266)

#include <Arduino.h>

// ESP8266 core's global `ESP` (EspClass, declared by Arduino.h itself via
// core_esp8266_features.h) already exposes restart() — unlike ESP32, there
// is no separate esp_system.h to include here.

uint32_t ArduinoClock::monotonicMillis() const { return millis(); }

void ArduinoClock::blockingDelayMs(uint32_t ms) const { delay(ms); }

void ArduinoClock::restartDevice() {
  delay(100);
  ESP.restart();
}

#endif  // ARDUINO_ARCH_ESP8266
