#pragma once

// Arduino compiles every .cpp under src/ for every sketch regardless of
// target board. This composition root declares concrete members of every
// ESP8266-only adapter type (several of which are themselves guarded out
// on other cores -- see Esp8266ArduinoMqttTransport.h/
// Esp8266SoftApProvisioningTransport.h), so the whole class must no-op
// outside an ESP8266 build rather than fail with "incomplete type" errors.
#if defined(ARDUINO_ARCH_ESP8266)

#include "../../core/Dependencies.h"
#include "ArduinoClock.h"
#include "ArduinoLogger.h"
#include "Esp8266ArduinoHardwareIdentity.h"
#include "Esp8266ArduinoHttpClient.h"
#include "Esp8266ArduinoMqttTransport.h"
#include "Esp8266ArduinoNetwork.h"
#include "Esp8266ArduinoSecretStore.h"
#include "Esp8266ArduinoStorage.h"
#include "Esp8266SoftApProvisioningTransport.h"

// Default composition root for ESP8266 Arduino (IoTLabs internal design notes),
// mirroring Esp32ArduinoPlatform's shape exactly: owns one instance of
// every adapter and hands out an IoTLabsDependencies bundle pointing at
// them. This is the only place in the library that knows every port has
// exactly one ESP8266/Arduino-backed implementation.
class Esp8266ArduinoPlatform {
 public:
  Esp8266ArduinoPlatform();

  IoTLabsDependencies dependencies();

 private:
  ArduinoClock _clock;
  ArduinoLogger _logger;
  Esp8266ArduinoNetwork _network;
  Esp8266ArduinoHttpClient _http;
  Esp8266ArduinoMqttTransport _mqtt;
  Esp8266ArduinoStorage _storage;
  Esp8266ArduinoSecretStore _secrets;
  Esp8266ArduinoHardwareIdentity _identity;
  Esp8266SoftApProvisioningTransport _provisioning;
};

#endif  // ARDUINO_ARCH_ESP8266
