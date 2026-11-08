#pragma once

#if !defined(ARDUINO)

#include "../../core/Dependencies.h"
#include "LinuxClock.h"
#include "LinuxHardwareIdentity.h"
#include "LinuxHttpClient.h"
#include "LinuxLogger.h"
#include "LinuxMqttTransport.h"
#include "LinuxNetwork.h"
#include "LinuxProvisioningTransport.h"
#include "LinuxSecretStore.h"
#include "LinuxStorage.h"

// Default composition root for native Linux/POSIX
// (IoTLabs internal design notes), same shape as
// Esp32ArduinoPlatform: owns one instance of every adapter and hands out an
// IoTLabsDependencies bundle pointing at them. This is the only place in
// the Linux adapter set that knows every port has exactly one Linux-backed
// implementation.
class LinuxPlatform {
 public:
  LinuxPlatform();

  IoTLabsDependencies dependencies();

 private:
  LinuxClock _clock;
  LinuxLogger _logger;
  LinuxNetwork _network;
  LinuxHttpClient _http;
  LinuxMqttTransport _mqtt;
  LinuxStorage _storage;
  LinuxSecretStore _secrets;
  LinuxHardwareIdentity _identity;
  LinuxProvisioningTransport _provisioning;
};

#endif  // !ARDUINO
