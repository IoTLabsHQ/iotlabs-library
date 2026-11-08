#pragma once

// Every .h/.cpp under src/platform/<x>/ self-guards to an empty translation
// unit outside its own build: Arduino's library build system compiles every
// .cpp under src/ for every sketch regardless of target board, so without
// this guard an ESP32/ESP8266/RP2040 sketch build would try to pull in this
// POSIX-only adapter too. Arduino always defines the `ARDUINO` macro; its
// *absence* is what marks a native (CMake) build, unlike the sibling
// platform directories which key off an arch-specific macro
// (ARDUINO_ARCH_ESP32, ...) because they only ever build under Arduino.
#if !defined(ARDUINO)

#include "../../core/ports/IClock.h"

// std::chrono::steady_clock-backed IClock for native Linux/POSIX builds.
class LinuxClock : public IClock {
 public:
  uint32_t monotonicMillis() const override;
  void blockingDelayMs(uint32_t ms) const override;

  // A Linux daemon cannot safely self-exec/replace its own running process
  // image the way ESP.restart() resets an MCU in place. Instead this calls
  // std::exit() with a distinct, documented exit code (see LinuxClock.cpp)
  // and logs that the process expects an external supervisor (systemd
  // `Restart=always`/`Restart=on-failure`, a container orchestrator, etc.)
  // to actually restart it. This is NOT an in-place restart -- do not treat
  // it as one.
  void restartDevice() override;

  // Exit code passed to std::exit() by restartDevice(). Chosen in the
  // reserved-for-application-use range (>63, avoiding common shell/signal
  // convention codes 0-63) and documented here so a supervisor's restart
  // policy can special-case it if desired.
  static constexpr int kRestartRequestedExitCode = 75;
};

#endif  // !ARDUINO
