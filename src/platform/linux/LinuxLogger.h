#pragma once

#if !defined(ARDUINO)

#include "../../core/ports/ILogger.h"

// Writes every line to stderr. Deliberately minimal: stderr is always
// available, is trivially redirectable, and is captured by journald when
// running under systemd -- a dedicated syslog/journal integration is out of
// scope for this candidate adapter (see BOARD_QUALIFICATION.md).
class LinuxLogger : public ILogger {
 public:
  void log(const char* line) override;
};

#endif  // !ARDUINO
