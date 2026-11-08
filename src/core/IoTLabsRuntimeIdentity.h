#pragma once

#include <cstdint>
#include <string>

// Identity handed back by the Device Platform during activation. Internal
// only (never crosses the public IoTLabs.h boundary) so it is free to use
// std::string instead of the public API's const-char*/String conventions.
struct IoTLabsRuntimeIdentity {
  std::string deviceId;
  std::string deviceTemplateId;
  uint32_t deviceTemplateVersion = 1;

  std::string mqttClientId;
  std::string mqttUsername;
  std::string mqttPassword;
  std::string mqttBroker;
  uint16_t mqttPort = 8883;
  std::string topicPrefix;

  bool valid() const {
    return !deviceId.empty() && !mqttClientId.empty() &&
           !mqttUsername.empty() && !mqttPassword.empty() &&
           !mqttBroker.empty();
  }
};
