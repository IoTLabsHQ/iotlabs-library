#include "Rp2040ArduinoHardwareIdentity.h"

#if defined(ARDUINO_ARCH_RP2040)

#include <Arduino.h>
#include <cstring>

// rp2040.getChipID() (RP2040Support.h, pulled in by Arduino.h) already
// returns the Pico SDK's per-chip pico_get_unique_board_id_string() as a
// NUL-terminated hex string — the Arduino-level wrapper the task brief asks
// to prefer over calling pico/unique_id.h directly.
void Rp2040ArduinoHardwareIdentity::readHardwareId(char* buffer, size_t bufferSize) const {
  if (!buffer || bufferSize == 0) return;
  const char* chipId = rp2040.getChipID();
  strncpy(buffer, chipId, bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

#endif  // ARDUINO_ARCH_RP2040
