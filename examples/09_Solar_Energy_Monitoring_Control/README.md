# Advanced — Solar Energy Monitoring & Control

**Trạng thái: DRAFT — compile PASS thật, chưa chạy trên phần cứng thật.** Firmware đã viết và
compile thành công (xem "Build" bên dưới), nhưng chưa có run thật trên board + Console/Mobile —
chủ dự án sẽ tự thực hiện QA trên phần cứng thật và confirm trạng thái cuối cùng, xem
tài liệu nội bộ của IoTLabs.

Nguồn ý tưởng: [maker.iotlabs.vn — Hệ thống giám sát và điều khiển năng lượng mặt trời với
ESP32](https://maker.iotlabs.vn/projects/he-thong-giam-sat-va-dieu-khien-nang-luong-mat-troi-voi-esp32).

## PHẠM VI AN TOÀN — CHỈ DC ĐIỆN ÁP THẤP (đọc trước khi đấu dây)

Ví dụ này nhắm tới hệ pin mặt trời DC điện áp thấp, lớp 6-12V: tấm PV → INA219 → mạch nạp/ắc quy →
tải DC. **Nằm ngoài phạm vi rõ ràng:**

- Điện lưới/AC 220V dưới bất kỳ hình thức nào.
- Inverter hoặc thiết bị hoà lưới (grid-tie).
- Bất kỳ chuỗi/tấm DC nào vượt quá 50V.

Không tự ý áp dụng firmware/sơ đồ đấu dây này cho hệ điện áp cao hơn hoặc hệ AC mà không thiết kế
lại toàn bộ bởi người có chuyên môn phù hợp. Xem
tài liệu nội bộ của IoTLabs cho phát biểu phạm
vi đầy đủ.

Thư viện Arduino Library Manager cần cài thêm: `Adafruit INA219` (by Adafruit) — tự kéo theo
`Adafruit BusIO` như một dependency bắc cầu qua Arduino Library Manager.

## Build

Board: **ESP32 DevKit cổ điển** (không phải ESP32-C3 Supermini dùng ở ví dụ 07/08) — FQBN
`esp32:esp32:esp32`.

```bash
arduino-cli lib install "Adafruit INA219"
arduino-cli compile --fqbn esp32:esp32:esp32 --library <repo-root> examples/09_Solar_Energy_Monitoring_Control
arduino-cli upload  --fqbn esp32:esp32:esp32 --library <repo-root> -p <PORT> examples/09_Solar_Energy_Monitoring_Control
```

Compile PASS thật (2026-08-18), verify bằng `arduino-cli compile --fqbn esp32:esp32:esp32`:

```text
Sketch uses 1198396 bytes (91%) of program storage space. Maximum is 1310720 bytes.
Global variables use 50016 bytes (15%) of dynamic memory, leaving 277664 bytes for local variables. Maximum is 327680 bytes.
```

## Phần cứng (ESP32 DevKit)

| Chức năng | GPIO | Ghi chú |
|---|---|---|
| INA219 (I2C — điện áp/dòng/công suất PV) SDA | 21 | Chân `Wire` mặc định của ESP32 |
| INA219 SCL | 22 | Chân `Wire` mặc định của ESP32, địa chỉ I2C mặc định `0x40` |
| Điện áp ắc quy (ADC1, chỉ đọc — tránh xung đột ADC2/WiFi) | 34 | Qua cầu phân áp điện trở — **PHẢI CALIBRATE** với điện trở thật đang lắp, xem mục Calibration ở `01_SPEC_KIEN_TRUC_HE_THONG.md` |
| Relay/MOSFET điều khiển tải DC (output số) | 25 | Boot mặc định LOW (tải OFF) cho tới khi AUTO/MANUAL quyết định |

## Controls

Console/Mobile render trực tiếp từ Control Type Registry — xem `PROFILE_INTENT.json`:

| Control ID | typeKey | access | config | Hiển thị |
|---|---|---|---|---|
| `control_1_value` (Điện áp PV) | `value` | `read` | `{"unit":"V"}` | Số liệu chỉ đọc |
| `control_2_value` (Dòng điện PV) | `value` | `read` | `{"unit":"mA"}` | Số liệu chỉ đọc |
| `control_3_value` (Công suất PV) | `value` | `read` | `{"unit":"mW"}` | Số liệu chỉ đọc |
| `control_4_value` (Điện áp Ắc quy) | `value` | `read` | `{"unit":"V","min":0,"max":15}` | **Gauge** (có `min`+`max`) |
| `control_5_on_off` (Tải DC) | `on_off` | `read_write` | — | Công tắc bật/tắt |
| `control_6_select` (Chế độ vận hành) | `select` | `read_write` | `{"options":["auto","manual"]}` | Dropdown |

## Automation — AUTO/MANUAL (low-voltage disconnect/reconnect)

- **AUTO (mặc định):** hysteresis kinh điển — `battery_voltage <= BATTERY_LVD_VOLTS` (mặc định
  11.0V) → ép tải OFF; `battery_voltage >= BATTERY_LVR_VOLTS` (mặc định 12.4V, luôn phải lớn hơn
  LVD để tránh chattering) → cho phép tải ON trở lại. Giữa 2 ngưỡng, tải giữ nguyên trạng thái hiện
  tại.
- **MANUAL:** tải hoàn toàn do người dùng điều khiển qua `control_5_on_off`; ngưỡng AUTO không được
  áp dụng khi đang MANUAL.
- Ghi `control_5_on_off` trong khi đang AUTO → bị bỏ qua, thiết bị report lại trạng thái thật hiện
  tại (AUTO sở hữu relay).
- `BATTERY_LVD_VOLTS`/`BATTERY_LVR_VOLTS` là hằng `#define`/`constexpr` — **PHẢI tinh chỉnh** theo
  hoá học/lớp điện áp ắc quy thật (6V/9V/12V khác nhau); giá trị mặc định trong code chỉ là ví dụ
  cho ắc quy chì-axit 12V.

Chi tiết đầy đủ state machine: tài liệu nội bộ của IoTLabs.

## Ghi chú kiểm thử

Chưa chạy thử trên phần cứng thật — xem
tài liệu nội bộ của IoTLabs cho danh sách
test case cụ thể, và `04_QA_TEST_PLAN.md` cho checklist đầy đủ theo từng tầng. Chủ dự án tự thực
hiện QA trên phần cứng thật (bao gồm calibrate cầu phân áp ắc quy bằng đồng hồ vạn năng thật) và tự
confirm trạng thái cuối cùng trước khi ví dụ này được coi là Verified.
