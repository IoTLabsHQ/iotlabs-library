#include "Esp8266ArduinoPlatform.h"

// Esp8266ArduinoPlatform.h itself no-ops outside ESP8266 builds, so guard
// this .cpp the same way.
#if defined(ARDUINO_ARCH_ESP8266)

Esp8266ArduinoPlatform::Esp8266ArduinoPlatform() : _secrets(_storage) {}

IoTLabsDependencies Esp8266ArduinoPlatform::dependencies() {
  IoTLabsDependencies deps;
  deps.clock = &_clock;
  deps.network = &_network;
  deps.http = &_http;
  deps.mqtt = &_mqtt;
  deps.storage = &_storage;
  deps.secrets = &_secrets;
  deps.identity = &_identity;
  deps.provisioning = &_provisioning;
  deps.logger = &_logger;
  return deps;
}

#endif  // ARDUINO_ARCH_ESP8266
