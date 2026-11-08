#include "Stm32ArduinoPlatform.h"

#if defined(ARDUINO_ARCH_STM32)

Stm32ArduinoPlatform::Stm32ArduinoPlatform() : _secrets(_storage) {}

IoTLabsDependencies Stm32ArduinoPlatform::dependencies() {
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

#endif  // ARDUINO_ARCH_STM32
