# QuickStart — Relay (AutoPinMapping variant)

Same device as [`01_QuickStart_Relay`](../01_QuickStart_Relay) — one `on_off` Control bound to a relay — rewritten with `iot.setPinMapping()` instead of hand-written `pinMode`/`digitalWrite`/`iot.report`/`iot.on` wiring.

`setPinMapping()` is an **optional** convenience layer: `on()`/`report()` still work exactly as before for anything it doesn't cover (e.g. sensors read through their own driver library). See the original example if you'd rather wire the pin by hand.

Check the GPIO used by the sketch and whether your relay module is active HIGH or active LOW before testing.
