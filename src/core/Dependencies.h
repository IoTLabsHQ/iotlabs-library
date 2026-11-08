#pragma once

#include "ports/IClock.h"
#include "ports/IHardwareIdentity.h"
#include "ports/IHttpClient.h"
#include "ports/ILogger.h"
#include "ports/IMqttTransport.h"
#include "ports/INetwork.h"
#include "ports/IProvisioningTransport.h"
#include "ports/ISecretStore.h"
#include "ports/IStorage.h"

// Composition-root bundle (IoTLabs internal design notes).
// Normal sketches never build this by hand: `IoTLabs iot;` selects a default
// platform factory. Advanced/test code may construct one explicitly and pass
// it to the `IoTLabs(IoTLabsDependencies)` constructor.
struct IoTLabsDependencies {
  IClock* clock = nullptr;
  INetwork* network = nullptr;
  IHttpClient* http = nullptr;
  IMqttTransport* mqtt = nullptr;
  IStorage* storage = nullptr;
  ISecretStore* secrets = nullptr;
  IHardwareIdentity* identity = nullptr;
  IProvisioningTransport* provisioning = nullptr;
  ILogger* logger = nullptr;

  bool complete() const {
    return clock && network && http && mqtt && storage && secrets && identity && provisioning && logger;
  }
};
