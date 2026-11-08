#include "LinuxLogger.h"

#if !defined(ARDUINO)

#include <cstdio>

void LinuxLogger::log(const char* line) {
  if (!line) return;
  std::fprintf(stderr, "%s\n", line);
  std::fflush(stderr);
}

#endif  // !ARDUINO
