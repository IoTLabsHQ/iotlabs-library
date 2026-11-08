#pragma once

#include <string>

// Builds the four MQTT Runtime Protocol v1 topics from the identity handed
// back by activation (IoTLabs internal design notes).
class TopicResolver {
 public:
  void build(const std::string& topicPrefix, const std::string& mqttClientId);

  const std::string& telemetry() const { return _telemetry; }
  const std::string& cmd() const { return _cmd; }
  const std::string& status() const { return _status; }
  const std::string& ping() const { return _ping; }

 private:
  std::string _telemetry;
  std::string _cmd;
  std::string _status;
  std::string _ping;
};
