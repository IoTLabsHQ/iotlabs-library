#include "Esp32ArduinoHardwareIdentity.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <cstdio>
#include <esp_system.h>

void Esp32ArduinoHardwareIdentity::readHardwareId(char* buffer, size_t bufferSize) const {
  if (!buffer || bufferSize == 0) return;
  const uint64_t chip = ESP.getEfuseMac();
  snprintf(buffer, bufferSize, "%04X%08X", static_cast<uint16_t>(chip >> 32), static_cast<uint32_t>(chip));
}

#endif  // ARDUINO_ARCH_ESP32
