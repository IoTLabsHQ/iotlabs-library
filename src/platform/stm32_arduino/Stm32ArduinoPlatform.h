#pragma once

#if defined(ARDUINO_ARCH_STM32)

#include "../../core/Dependencies.h"
#include "Stm32ArduinoClock.h"
#include "Stm32ArduinoHardwareIdentity.h"
#include "Stm32ArduinoHttpClient.h"
#include "Stm32ArduinoLogger.h"
#include "Stm32ArduinoMqttTransport.h"
#include "Stm32ArduinoNetwork.h"
#include "Stm32ArduinoSecretStore.h"
#include "Stm32ArduinoStorage.h"
#include "Stm32ProvisioningTransport.h"

// Default composition root for the STM32 Arduino candidate platform
// (IoTLabs internal design notes), qualified so far
// only against a Nucleo-144 NUCLEO_F429ZI board with its on-board Ethernet
// PHY — see IoTLabs internal design notes Owns one
// instance of every adapter and hands out an IoTLabsDependencies bundle
// pointing at them, mirroring Esp32ArduinoPlatform's shape exactly.
class Stm32ArduinoPlatform {
 public:
  Stm32ArduinoPlatform();

  IoTLabsDependencies dependencies();

 private:
  Stm32ArduinoClock _clock;
  Stm32ArduinoLogger _logger;
  Stm32ArduinoNetwork _network;
  Stm32ArduinoHttpClient _http;
  Stm32ArduinoMqttTransport _mqtt;
  Stm32ArduinoStorage _storage;
  Stm32ArduinoSecretStore _secrets;
  Stm32ArduinoHardwareIdentity _identity;
  Stm32ProvisioningTransport _provisioning;
};

#endif  // ARDUINO_ARCH_STM32
