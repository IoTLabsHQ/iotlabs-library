#pragma once

#include <cstdint>

// Monotonic time + device lifecycle control. Grouped together because every
// platform that can report uptime can also reset itself; splitting this into
// two ports would not buy any real substitutability for supported targets.
class IClock {
 public:
  virtual ~IClock() = default;

  virtual uint32_t monotonicMillis() const = 0;
  virtual void blockingDelayMs(uint32_t ms) const = 0;
  virtual void restartDevice() = 0;
};
