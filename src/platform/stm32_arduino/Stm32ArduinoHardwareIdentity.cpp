#include "Stm32ArduinoHardwareIdentity.h"

#if defined(ARDUINO_ARCH_STM32)

#include <Arduino.h>
#include <cstdio>

// HAL_GetUIDw0/w1/w2() (declared in the family's stm32<fam>xx_hal.h, pulled
// in transitively by Arduino.h on this core) read the chip's genuine 96-bit
// factory-programmed unique device ID register. Unlike a higher-level
// STM32.getUID() convenience wrapper (which does not exist on core 3.0.0),
// these three low-level HAL calls are guaranteed present on every STM32
// family this core supports, so no board/family #ifdef is needed here.
void Stm32ArduinoHardwareIdentity::readHardwareId(char* buffer, size_t bufferSize) const {
  if (!buffer || bufferSize == 0) return;
  const uint32_t w0 = HAL_GetUIDw0();
  const uint32_t w1 = HAL_GetUIDw1();
  const uint32_t w2 = HAL_GetUIDw2();
  snprintf(buffer, bufferSize, "%08lX%08lX%08lX", static_cast<unsigned long>(w0), static_cast<unsigned long>(w1),
           static_cast<unsigned long>(w2));
}

#endif  // ARDUINO_ARCH_STM32
