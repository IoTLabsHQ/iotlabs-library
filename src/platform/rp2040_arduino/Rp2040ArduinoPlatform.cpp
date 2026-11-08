#include "Rp2040ArduinoPlatform.h"

#if defined(ARDUINO_ARCH_RP2040)

Rp2040ArduinoPlatform::Rp2040ArduinoPlatform() : _secrets(_storage) {}

IoTLabsDependencies Rp2040ArduinoPlatform::dependencies() {
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

#endif  // ARDUINO_ARCH_RP2040
