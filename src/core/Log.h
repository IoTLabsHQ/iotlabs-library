#pragma once

#include <cstdarg>
#include <cstdio>

#include "ports/ILogger.h"

// Bounded printf-style formatting on top of ILogger::log(const char*), so
// core never touches Serial/Print directly (IoTLabs internal design notes).
namespace iotlabs_log {

inline void line(ILogger* logger, const char* fmt, ...) {
  if (!logger) return;
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  logger->log(buffer);
}

}  // namespace iotlabs_log
