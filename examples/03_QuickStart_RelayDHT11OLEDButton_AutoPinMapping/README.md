# QuickStart — Relay + DHT11 + OLED + Button (AutoPinMapping variant)

Same device as [`03_QuickStart_RelayDHT11OLEDButton`](../03_QuickStart_RelayDHT11OLEDButton) — Relay, DHT11 temperature/humidity, OLED status display, physical button — with the **relay** rewritten to `iot.setPinMapping()`.

The button stays hand-wired on purpose: it reports a JSON event and also accepts a remote command that re-triggers the same local action, which `setPinMapping()`'s debounced-input mode doesn't cover (it only auto-reports a bool state or a fixed string). DHT/OLED are unaffected either way — they're driven through their own libraries, not a raw pin. See the `.ino` file's top comment for the full reasoning.

`setPinMapping()` is an **optional** convenience layer: `on()`/`report()` still work exactly as before for anything it doesn't cover.

Additional Arduino Library Manager dependencies: DHT sensor library by Adafruit, Adafruit GFX Library, Adafruit SSD1306.
