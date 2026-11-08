#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include "../../core/Dependencies.h"
#include "ArduinoClock.h"
#include "ArduinoLogger.h"
#include "Esp32ArduinoHardwareIdentity.h"
#include "Esp32ArduinoHttpClient.h"
#include "Esp32ArduinoMqttTransport.h"
#include "Esp32ArduinoNetwork.h"
#include "Esp32ArduinoSecretStore.h"
#include "Esp32ArduinoStorage.h"
#include "Esp32SoftApProvisioningTransport.h"

// Default composition root for ESP32 Arduino (IoTLabs internal design notes):
// owns one instance of every adapter and hands out an IoTLabsDependencies
// bundle pointing at them. This is the only place in the library that knows
// every port has exactly one ESP32/Arduino-backed implementation.
class Esp32ArduinoPlatform {
 public:
  Esp32ArduinoPlatform();

  IoTLabsDependencies dependencies();

 private:
  ArduinoClock _clock;
  ArduinoLogger _logger;
  Esp32ArduinoNetwork _network;
  Esp32ArduinoHttpClient _http;
  Esp32ArduinoMqttTransport _mqtt;
  Esp32ArduinoStorage _storage;
  Esp32ArduinoSecretStore _secrets;
  Esp32ArduinoHardwareIdentity _identity;
  Esp32SoftApProvisioningTransport _provisioning;
};

#endif  // ARDUINO_ARCH_ESP32
