# Indoor Air Quality Monitor

CO2 (core — the "closed room" problem) + temperature/humidity monitor with
a local OLED display, a GOOD/MODERATE/POOR status LED + buzzer alert
(mutable), and IoTLabs Cloud reporting. PM2.5 is an **optional add-on**
(dust/smoke is a different problem than a stuffy closed room, see PRD
section 4.1) — auto-detected at boot; the device runs CO2-only if the
module isn't wired. Full PRD and supporting docs (wiring, firmware
structure, dashboard build guide, provisioning guide) live under
tài liệu nội bộ của IoTLabs.

Additional Arduino Library Manager dependencies: `SparkFun SCD4x Arduino
Library`, `Adafruit PM25 AQI Sensor`, `Adafruit GFX Library`, `Adafruit
SSD1306`, `Adafruit NeoPixel`.

## Hardware (ESP32 Dev Kit V1)

| Component | GPIO | Notes |
|---|---|---|
| SCD40/SCD41 (CO2 + Temp/Humidity, core) SDA/SCL | 21 / 22 | I2C, address `0x62` |
| OLED SSD1306 SDA/SCL | 21 / 22 | shared bus with SCD4x, address `0x3C` |
| PM2.5 (PMS5003/PMSA003, **optional**) RX2 | 16 | ESP32 receives, wired to module TX. Leave unwired to run CO2-only. |
| PM2.5 TX2 | 17 | spare — module runs its own active/streaming mode |
| Mode/mute button | 27 | `INPUT_PULLUP`, active LOW, 50ms debounce |
| Buzzer | 4 | active HIGH |
| Status LED (WS2812) | 5 | 1 pixel — green/yellow/red for GOOD/MODERATE/POOR |
| BOOT button | 0 | hold 10s to factory-reset |

## Controls

| Control | typeKey | access | config |
|---|---|---|---|
| CO2 (core) | `value` | read | `{"unit":"ppm","min":400,"max":5000}` |
| PM2.5 (**optional** — only reported if the module is detected at boot) | `value` | read | `{"unit":"µg/m³","min":0,"max":500}` |
| Temperature | `value` | read | `{"unit":"C"}` |
| Humidity | `value` | read | `{"unit":"%","min":0,"max":100}` |
| Air Quality Status | `text` | read | value is `GOOD`\|`MODERATE`\|`POOR`, computed locally from CO2 (+ PM2.5 if present) |
| Buzzer Enable | `on_off` | read_write | |
| Mute Alert | `button` | trigger | `{"state":"pressed"}`, same effect as the physical button |

## Local-first behavior

Air Quality Status, the OLED display, and the buzzer/LED alert are computed
entirely on-device from the last sensor reading — none of them require
Cloud connectivity. Losing Wi-Fi/Cloud only stops `iot.report()` publishes;
sensing, display, and alerting keep working.

## Known gaps (V1)

1. **CO2/PM2.5 thresholds are reference values, not field-validated.**
   `CO2_MODERATE_PPM`/`CO2_POOR_PPM`/`PM25_MODERATE_UGM3`/`PM25_POOR_UGM3`
   in the `.ino` are WHO/US-EPA-style indoor bands, not tuned against this
   specific hardware yet — re-tune after real-world testing (see PRD GIAI
   DOAN 03).
2. **No stateful storage across reboot for app settings**, same gap as
   example `05_Advanced_SmartWatering` — `buzzerEnabled`/mute state are
   RAM-only and reset to defaults every boot.
3. **No captive-portal HTML for provisioning** — uses the library's bare
   JSON REST SoftAP directly (same as `00_QuickStart_Provisioning`); see
   tài liệu nội bộ của IoTLabs
   for the real Mobile-driven provisioning flow.

## Testing notes

Not yet run against real hardware — no SCD40/SCD41 or PMS5003/PMSA003
module was available at implementation time. Verified only:
`arduino-cli compile --fqbn esp32:esp32:esp32` against the real
`SparkFun SCD4x Arduino Library`/`Adafruit PM25 AQI Sensor` headers and this
repo's own `IoTLabs` library source — compiles clean, 0 warnings. Sensor
readings, pin mapping, and thresholds all need confirmation on real
hardware before this moves past GIAI DOAN 02 in the PRD.

Real `arduino-cli compile` output (default `esp32:esp32:esp32` partition
scheme):

```text
Sketch uses 1219445 bytes (93%) of program storage space. Maximum is 1310720 bytes.
Global variables use 50864 bytes (15%) of dynamic memory, leaving 276816 bytes for local variables. Maximum is 327680 bytes.
```

93% flash is tight — mostly TLS (`WiFiClientSecure`) + `ArduinoJson` +
`PubSubClient` (via `IoTLabs`) + `Adafruit_SSD1306`/`GFX` + the two sensor
libraries. If a future change needs more flash, switch the board's
partition scheme to a "Minimal SPIFFS"/no-OTA layout before trimming
features.
