# IoTLabs Library

IoTLabs Library is the C++ device library for connecting supported IoT boards and Linux devices to the IoTLabs Device Registry.

It provides a stable application API for device provisioning, activation, cloud connectivity, Controls, reported state, and device runtime communication while keeping platform-specific implementation details out of application code.

## Quick start

```cpp
#include <IoTLabs.h>

IoTLabs iot;

void setup() {
  iot.begin();
}

void loop() {
  iot.loop();
}
```

For a working project, open one of the sketches under `examples/`. Each example contains its own `README.md` describing what the example demonstrates and any extra libraries or hardware it needs.

## Installation

### Arduino IDE

Install the released ZIP with:

`Sketch → Include Library → Add .ZIP Library...`

Then open:

`File → Examples → IoTLabs`

### PlatformIO

The repository includes `library.json` so the same public C++ library can be consumed from PlatformIO projects.

## Public API

Application projects should depend only on the public IoTLabs interface exposed through:

```cpp
#include <IoTLabs.h>
```

Platform-specific implementation details are intentionally kept out of project code.

## Advanced composition

Most sketches only ever need `IoTLabs iot;`. Advanced projects and tests may inject their own port implementations (clock, network, MQTT transport, storage, ...) via `IoTLabs iot(dependencies);`. See `examples/04_Advanced_CustomDependencies`.

## Documentation

User guides for Arduino, PlatformIO, supported boards, provisioning, and other platforms are maintained separately from this source repository.
