# QuickStart — Servo Sweep (`onCommandFinish` demo)

**Status: DRAFT — compile PASS thật, chưa chạy trên phần cứng thật.** Verified with `arduino-cli compile` (ESP32-C3 SuperMini) (2026-08-25): flash 1,217,117 bytes (92%), RAM 40,360 bytes (12%). No physical servo/board run yet — see "Ghi chú kiểm thử" below.

Demonstrates the SDK's `IoTLabs::onCommandFinish(handler)` API added alongside this example: a hook that fires right after a write Control's handler has been dispatched successfully, just before the SDK's own automatic `publishStatus()`.

The point of this example is the distinction between two different moments in time for a command whose *effect* takes real time to complete:

1. **Command dispatched** (`onCommandFinish` fires here) — the servo hasn't necessarily moved yet.
2. **Command's effect actually finished** (only known later, from `loop()` polling the servo's real position) — this is when the SDK reports the final state.

The `Sweep` write Control's handler (`onSweepControl`) returns immediately: it only sets a target angle and starts a `millis()`-based non-blocking state machine, never calling `delay()`. `iot.onCommandFinish(...)` is registered but its callback deliberately does **not** call `report()`/`reportBool()` — doing so there would just reproduce the SDK's existing automatic sync-report behavior and defeat the purpose of the demo. The real "sweep finished" report (`Sweep` back to `false`, `Servo Angle` set to the angle actually reached) happens later, from `loop()`'s own `updateServoSweep()`, only once the servo has physically arrived at its target.

## Prerequisite — install ESP32Servo first

This example uses `#include <ESP32Servo.h>`, which is **not declared in any repo manifest** (checked: not a IoTLabs Library dependency). Install it yourself before compiling:

```bash
arduino-cli lib install "ESP32Servo"
```

## Hardware

Board: **ESP32-C3 SuperMini** — FQBN `esp32:esp32:nologo_esp32c3_super_mini`.

| Signal       | GPIO | Notes                                   |
|--------------|------|------------------------------------------|
| Servo signal | 4    | SG90-class hobby servo, external 5V supply recommended (don't power the servo from the board's 3.3V/5V pin under load) |

GPIO4 is used instead of a strapping pin (GPIO2/8/9 on ESP32-C3) to avoid interfering with boot mode selection.

## Build

```bash
arduino-cli lib install "ESP32Servo"
arduino-cli compile --fqbn esp32:esp32:nologo_esp32c3_super_mini --library <repo-root> examples/12_QuickStart_ServoSweep
arduino-cli upload  --fqbn esp32:esp32:nologo_esp32c3_super_mini --library <repo-root> -p <PORT> examples/12_QuickStart_ServoSweep
```

Or via the repo's wrapper script:

```bash
arduino-cli compile -b esp32:esp32:esp32c3 examples/12_QuickStart_ServoSweep
```

## Controls

See `PROFILE_INTENT.json` for the exact Control mapping.

| Control        | typeKey  | access       | Description |
|----------------|----------|--------------|-------------|
| Sweep          | on_off   | read_write   | Write `true` to start a non-blocking sweep to the opposite end of travel. Device reports it back to `false` once the servo has physically finished moving — not when the command was merely accepted. |
| Servo Angle    | value    | read         | Last angle (degrees, 0-180) the device actually reached. Reported only once a sweep finishes, from `loop()`. |

## Ghi chú kiểm thử

Compile-verified via `arduino-cli compile` in this session (see status line above for the actual result). Not yet run on real hardware — no physical servo/board QA has been performed; the servo motion, GPIO4 signal timing, and 5V power draw should be validated on real hardware before shipping this pattern to a device fleet.
