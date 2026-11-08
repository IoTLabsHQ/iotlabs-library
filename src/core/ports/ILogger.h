#pragma once

// Single-line diagnostic sink. Core builds a bounded, pre-formatted line and
// hands it here; adapters decide where it goes (Serial, syslog, stdout, ...).
class ILogger {
 public:
  virtual ~ILogger() = default;

  virtual void log(const char* line) = 0;
};
