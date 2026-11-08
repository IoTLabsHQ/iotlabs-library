#include "TopicResolver.h"

void TopicResolver::build(const std::string& topicPrefix, const std::string& mqttClientId) {
  std::string base = topicPrefix;
  if (base.empty()) base = std::string("devices/") + mqttClientId;
  while (!base.empty() && base.back() == '/') base.pop_back();

  _telemetry = base + "/telemetry";
  _cmd = base + "/cmd";
  _status = base + "/status";
  _ping = base + "/ping";
}
