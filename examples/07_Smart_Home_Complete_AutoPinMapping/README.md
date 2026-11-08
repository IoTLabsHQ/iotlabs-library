# Smart Home Complete (AutoPinMapping variant)

Same device as [`07_Smart_Home_Complete`](../07_Smart_Home_Complete) — relay + DHT11 climate sensing + status LED + boot button — with the **relay** rewritten to `iot.setPinMapping()`.

The boot button stays hand-wired on purpose: a short press both toggles the relay locally and reports a `"pressed"` event, and a long hold (>=10s) triggers a factory reset. `setPinMapping()`'s debounced-input mode only auto-reports (a bool state, or a fixed string on release) — it has no hook for "also toggle a different control" or hold-duration logic, so forcing this button through it would silently drop real behavior. DHT/LED are unaffected either way. See the `.ino` file's top comment for the full reasoning.

`setPinMapping()` is an **optional** convenience layer: `on()`/`report()` still work exactly as before for anything it doesn't cover.

Additional Arduino Library Manager dependencies: DHT sensor library by Adafruit, Adafruit NeoPixel.
