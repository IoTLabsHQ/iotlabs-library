#include "ArduinoClock.h"

// Every src/ .cpp is compiled for every sketch regardless of target board;
// guard the body so this file no-ops when compiling for a different arch
// (matches the ARDUINO_ARCH_ESP32 guard on the esp32_arduino adapters).
#if defined(ARDUINO_ARCH_RP2040)

#include <Arduino.h>

// RP2040Support.h (pulled in transitively by Arduino.h on arduino-pico)
// exposes the global `rp2040` helper object with restart()/getChipID(),
// mirroring how ESP32's ArduinoClock uses the global `ESP` object.

uint32_t ArduinoClock::monotonicMillis() const { return millis(); }

void ArduinoClock::blockingDelayMs(uint32_t ms) const { delay(ms); }

void ArduinoClock::restartDevice() {
  delay(100);
  rp2040.restart();
}

#endif  // ARDUINO_ARCH_RP2040
