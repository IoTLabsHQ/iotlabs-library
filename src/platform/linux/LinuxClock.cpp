#include "LinuxClock.h"

#if !defined(ARDUINO)

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
// Process-start epoch for monotonicMillis(), matching millis()'s
// "time since boot/reset" contract closely enough for the engine's
// reconnect/backoff/interval math (it only ever looks at deltas).
const std::chrono::steady_clock::time_point kProcessStart = std::chrono::steady_clock::now();
}  // namespace

uint32_t LinuxClock::monotonicMillis() const {
  const auto elapsed = std::chrono::steady_clock::now() - kProcessStart;
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

void LinuxClock::blockingDelayMs(uint32_t ms) const { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void LinuxClock::restartDevice() {
  std::fprintf(stderr,
               "[LinuxClock] restartDevice() requested: this process cannot restart itself in place. "
               "Exiting with code %d; an external supervisor (systemd Restart=always/on-failure, a "
               "container orchestrator, etc.) is expected to relaunch it.\n",
               kRestartRequestedExitCode);
  std::fflush(stderr);
  std::exit(kRestartRequestedExitCode);
}

#endif  // !ARDUINO
