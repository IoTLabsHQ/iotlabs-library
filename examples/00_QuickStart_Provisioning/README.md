# QuickStart — Provisioning

Minimal IoTLabs example showing the library lifecycle with `IoTLabs iot`, `iot.begin()` and `iot.loop()`.

Use this example to validate first-boot provisioning and connection to the IoTLabs Device Registry before adding hardware-specific application logic.

Update the profile/platform configuration required by the sketch, upload it to a supported board, then provision the device from IoTLabs Mobile.

## Device id & activation secret (compile-time macros)

Before `#include <IoTLabs.h>`, the sketch defines:

- `IOTLABS_DEVICE_ID` — **required**. Every sketch calling `IoTLabs::begin()` must set this to the real Device id copied from IoTLabs Console (format `DEV_xxxxxxxx`), created per physical unit/per user — example source can only show a placeholder, never a real shared default. It drives the SoftAP's WiFi network name: the board broadcasts an AP named `IoTLabs-<last 6 chars of your Device id>` during setup, predictable and shown in Console before you even flash the device. Missing or empty → `begin()` fails and `lastError()` returns `"DEVICE_ID_REQUIRED"`.
- `IOTLABS_DEVICE_ACTIVATION_SECRET` — optional dev/test shortcut. Paste the one-time activation token copied from Console when starting a provisioning session, and the SoftAP captive-portal setup page (`http://192.168.4.1/`) skips asking for the "Activation Token" field and auto-uses this value — saves retyping it into the portal on every reflash while iterating. Leave it undefined to keep entering the token by hand in the portal, as before.
- `IOTLABS_WIFI_SSID` / `IOTLABS_WIFI_PASSWORD` — optional, same idea for WiFi: when `IOTLABS_WIFI_SSID` is set, the portal also hides its "WiFi SSID"/"WiFi Password" fields and auto-fills them (leave `IOTLABS_WIFI_PASSWORD` undefined/empty for an open network). Set **both** this pair and `IOTLABS_DEVICE_ACTIVATION_SECRET` together on a fresh device and the SoftAP portal isn't needed at all — the board connects to WiFi and activates itself automatically at boot, with zero manual/captive-portal interaction (`ProvisioningManager::tryAutoProvision()`). The SoftAP still starts as a fallback, it's just never needed.
