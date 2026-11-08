#pragma once

// Every src/ .cpp is compiled for every sketch regardless of target board;
// this whole composition-root header is guarded so it (and the
// platform-only headers it pulls in) no-ops when parsed while compiling for
// a different arch, matching the ARDUINO_ARCH_ESP32 guard convention used on
// the esp32_arduino adapters.
#if defined(ARDUINO_ARCH_RP2040)

#include "../../core/Dependencies.h"
#include "ArduinoClock.h"
#include "ArduinoLogger.h"
#include "Rp2040ArduinoHardwareIdentity.h"
#include "Rp2040ArduinoHttpClient.h"
#include "Rp2040ArduinoMqttTransport.h"
#include "Rp2040ArduinoNetwork.h"
#include "Rp2040ArduinoSecretStore.h"
#include "Rp2040ArduinoStorage.h"
#include "Rp2040SoftApProvisioningTransport.h"

// Default composition root for RP2040 Arduino / Pico W (earlephilhower
// arduino-pico core), mirroring Esp32ArduinoPlatform.h's shape: owns one
// instance of every adapter and hands out an IoTLabsDependencies bundle
// pointing at them. Requires a WiFi-capable board (Pico W / Pico 2 W) since
// INetwork/IMqttTransport/IHttpClient/IProvisioningTransport all need the
// CYW43 radio; a plain (non-W) Pico has no radio and cannot back this
// platform (see IoTLabs internal design notes's
// "not every platform has Wi-Fi" principle).
class Rp2040ArduinoPlatform {
 public:
  Rp2040ArduinoPlatform();

  IoTLabsDependencies dependencies();

 private:
  ArduinoClock _clock;
  ArduinoLogger _logger;
  Rp2040ArduinoNetwork _network;
  Rp2040ArduinoHttpClient _http;
  Rp2040ArduinoMqttTransport _mqtt;
  Rp2040ArduinoStorage _storage;
  Rp2040ArduinoSecretStore _secrets;
  Rp2040ArduinoHardwareIdentity _identity;
  Rp2040SoftApProvisioningTransport _provisioning;
};

#endif  // ARDUINO_ARCH_RP2040
