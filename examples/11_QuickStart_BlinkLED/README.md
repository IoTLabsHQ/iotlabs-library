# QuickStart — Blink LED (ESP32-C3 SuperMini)

Shows how a project binds an IoTLabs `on_off` Control to the ESP32-C3 SuperMini's built-in LED (GPIO8, active LOW).

The application owns the GPIO and LED logic. IoTLabs Library owns provisioning, activation, cloud connectivity, Control routing and reported-state transport.

`IOTLABS_DEVICE_REGISTRY_URL` is set for local/testing use — remove or point it at the production host before shipping.
