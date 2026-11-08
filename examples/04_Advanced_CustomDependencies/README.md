# Advanced — Custom Dependencies

Shows the advanced composition constructor added by the Phase 1 ports/adapters refactor: `IoTLabs iot(dependencies);`.

IoTLabs is built from small capability ports (`IClock`, `INetwork`, `IHttpClient`, `IMqttTransport`, `IStorage`, `ISecretStore`, `IHardwareIdentity`, `IProvisioningTransport`, `ILogger`) under `core/ports/`. Normal sketches never see this — `IoTLabs iot;` selects the default ESP32 Arduino adapters for you.

This sketch reuses those default adapters (`Esp32ArduinoPlatform`) for everything except logging, and supplies its own `ILogger` that prefixes every SDK log line and blinks the onboard LED once per line. The same pattern is how a host-side unit test would substitute a fake `IClock`/`IMqttTransport`/... without a real board.

Check the GPIO used for the relay and the onboard LED before testing; the LED pin in particular varies across ESP32-C3 SuperMini board revisions.
