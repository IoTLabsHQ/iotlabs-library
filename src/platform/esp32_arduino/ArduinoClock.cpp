#include "ArduinoClock.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <esp_system.h>

uint32_t ArduinoClock::monotonicMillis() const { return millis(); }

void ArduinoClock::blockingDelayMs(uint32_t ms) const { delay(ms); }

void ArduinoClock::restartDevice() {
  delay(100);
  ESP.restart();
}

#endif  // ARDUINO_ARCH_ESP32
