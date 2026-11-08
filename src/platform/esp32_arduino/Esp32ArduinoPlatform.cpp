#include "Esp32ArduinoPlatform.h"

#if defined(ARDUINO_ARCH_ESP32)

Esp32ArduinoPlatform::Esp32ArduinoPlatform() : _secrets(_storage) {}

IoTLabsDependencies Esp32ArduinoPlatform::dependencies() {
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

#endif  // ARDUINO_ARCH_ESP32
