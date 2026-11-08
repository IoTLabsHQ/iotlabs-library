#include "Esp8266ArduinoHardwareIdentity.h"

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board; this file uses ESP.getChipId(), which does not exist on
// ESP32's EspClass, so it must no-op outside an ESP8266 build.
#if defined(ARDUINO_ARCH_ESP8266)

#include <Arduino.h>
#include <cstdio>

// ESP8266 has no ESP32-style efuse MAC API. Two candidates exist:
//   - ESP.getChipId() -- a factory-programmed 32-bit id (derived from the
//     lower 3 bytes of the base MAC address by the SDK), readable at boot
//     with no radio/WiFi state required.
//   - WiFi.macAddress() -- the 48-bit station MAC, but only meaningful
//     once WiFi has been initialized (beginStation()/mode set), which is
//     later in the boot sequence than IHardwareIdentity is needed (device
//     identity must be stable before provisioning even starts).
// getChipId() is picked: it is available immediately at boot, requires no
// WiFi side effects, and is stable/unique per chip -- exactly what
// hardwareId() (used for device claiming/activation) needs. It carries
// less entropy (32 bits vs. ESP32's 48-bit efuse MAC), which is an
// accepted tradeoff for this platform per 02_ESP8266_ARDUINO.md.
void Esp8266ArduinoHardwareIdentity::readHardwareId(char* buffer, size_t bufferSize) const {
  if (!buffer || bufferSize == 0) return;
  const uint32_t chipId = ESP.getChipId();
  snprintf(buffer, bufferSize, "%08X", chipId);
}

#endif  // ARDUINO_ARCH_ESP8266
