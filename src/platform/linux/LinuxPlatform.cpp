#include "LinuxPlatform.h"

#if !defined(ARDUINO)

LinuxPlatform::LinuxPlatform() : _secrets(_storage) {}

IoTLabsDependencies LinuxPlatform::dependencies() {
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

#endif  // !ARDUINO
