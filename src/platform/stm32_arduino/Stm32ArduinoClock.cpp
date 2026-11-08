#include "Stm32ArduinoClock.h"

#if defined(ARDUINO_ARCH_STM32)

#include <Arduino.h>

uint32_t Stm32ArduinoClock::monotonicMillis() const { return millis(); }

void Stm32ArduinoClock::blockingDelayMs(uint32_t ms) const { delay(ms); }

void Stm32ArduinoClock::restartDevice() {
  // STM32duino has no ESP.restart()-style helper. NVIC_SystemReset() is a
  // CMSIS core function available on every ARM Cortex-M target this core
  // supports, so it needs no family-specific #ifdef unlike the UID readout
  // in Stm32ArduinoHardwareIdentity.
  delay(100);
  NVIC_SystemReset();
}

#endif  // ARDUINO_ARCH_STM32
