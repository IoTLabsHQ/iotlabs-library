# Advanced — Multi Devices Control

**Trạng thái: DRAFT — compile PASS thật, chưa chạy trên phần cứng thật.** Cả 5 firmware đã viết và
compile thành công (xem "Build" bên dưới), nhưng chưa có run thật trên board + Console/Mobile —
chủ dự án sẽ tự thực hiện QA trên phần cứng thật và confirm trạng thái cuối cùng, xem
tài liệu nội bộ của IoTLabs và `08_QA_TEST_CASES.md`.

**5 firmware độc lập**, mỗi thiết bị 1 project con trong thư mục này — không có project `.ino` nào
ở cấp `08_Multi_Devices_Control/` (khác các ví dụ trước chỉ có 1 firmware). Mỗi thiết bị là 1 Device
**hoàn toàn độc lập** trên Device Registry — không gateway, không phụ thuộc thiết bị khác:

| Thư mục | Thiết bị | Năng lực chính |
|---|---|---|
| `SmartPlug/` | Ổ cắm thông minh | Bật/tắt + nút vật lý |
| `SmartLamp/` | Đèn thông minh | Bật/tắt + độ sáng PWM 0-100% + 4 chế độ chiếu sáng + nút vật lý |
| `DoorSecurity/` | Giám sát cửa | Trạng thái cửa + chế độ bảo vệ + còi cảnh báo + thống kê mở cửa |
| `AirQuality1/` | Chất lượng không khí phòng ngủ | CO₂/nhiệt độ/độ ẩm (SCD4x) + cảnh báo CO₂ |
| `AirQuality2/` | Chất lượng không khí phòng khách | Giống AirQuality1, thêm phát hiện CO₂ tăng nhanh do đông người |

Cả 5 thiết bị được **gộp hiển thị** (không gộp vận hành) trên **cùng 1 Dashboard** ở Console — thuần
tuý thao tác UI, không phải kiến trúc mạng. Chi tiết đầy đủ (kiến trúc, bảng Control, quyết định
scope Automation, hướng dẫn Console/Mobile) xem bộ tài liệu tại
tài liệu nội bộ của IoTLabs (`00_GIOI_THIEU.md` → `08_QA_TEST_CASES.md`).

## Build

Mỗi thiết bị compile riêng, trỏ `--library` vào thư mục gốc repo này:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> examples/08_Multi_Devices_Control/SmartPlug
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> examples/08_Multi_Devices_Control/SmartLamp
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> examples/08_Multi_Devices_Control/DoorSecurity
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> examples/08_Multi_Devices_Control/AirQuality1
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> examples/08_Multi_Devices_Control/AirQuality2
arduino-cli upload  --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> -p <PORT> examples/08_Multi_Devices_Control/<ThietBi>
```

`CDCOnBoot=cdc` **bắt buộc** trên ESP32-C3 Supermini — thiếu thì sketch vẫn chạy nhưng `Serial`
không bao giờ phản hồi qua cổng USB (gotcha đã xác nhận thật, xem README của ví dụ 07).

Compile PASS thật (2026-08-18), verify bằng `arduino-cli compile --fqbn esp32:esp32:esp32c3`:

| Thiết bị | Flash | RAM |
|---|---|---|
| SmartPlug | 1.199.627/1.310.720 byte (91%) | 37.644/327.680 byte (11%) |
| SmartLamp | 1.209.961/1.310.720 byte (92%) | 37.780/327.680 byte (11%) |
| DoorSecurity | 1.201.299/1.310.720 byte (91%) | 37.676/327.680 byte (11%) |
| AirQuality1 | 1.229.651/1.310.720 byte (93%) | 38.404/327.680 byte (11%) |
| AirQuality2 | 1.229.627/1.310.720 byte (93%) | 38.404/327.680 byte (11%) |

Thư viện Arduino Library Manager cần cài thêm: `Adafruit NeoPixel` (cả 5 thiết bị),
`SparkFun SCD4x Arduino Library` (chỉ AirQuality1/AirQuality2).

## Phần cứng (ESP32-C3 Supermini)

| Chức năng | GPIO | Thiết bị dùng |
|---|---|---|
| Relay/PWM (tải chính) | 4 | SmartPlug (relay), SmartLamp (PWM dimmer) |
| DoorSecurity door sensor | 4 | DoorSecurity — reed switch, INPUT_PULLUP |
| AirQuality SCD4x SDA | 4 | AirQuality1, AirQuality2 |
| DoorSecurity buzzer/alarm | 5 | DoorSecurity |
| AirQuality SCD4x SCL | 5 | AirQuality1, AirQuality2 |
| LED RGB (WS2812 onboard) | 8 | Cả 5 thiết bị — cần xác minh với board thật đang dùng, layout LED onboard khác nhau tuỳ nhà sản xuất clone |
| Nút BOOT onboard | 9 | Cả 5 thiết bị — giữ 10s = factory reset; SmartPlug/SmartLamp nhấn ngắn = đảo trạng thái tại chỗ |
| UART0 (USB-Serial) | 20 (RX) / 21 (TX) | Cả 5 thiết bị — nạp code/debug log |

GPIO4/5 dùng lại ở nhiều thiết bị vì mỗi thiết bị là 1 board vật lý riêng biệt — không xung đột.

## Provisioning — CẢNH BÁO quan trọng: dùng đúng flow

Dùng trang setup có sẵn của thư viện (`Esp32SoftApProvisioningTransport`, không viết
`provisioning_page.h` riêng — giống mọi ví dụ từ 07 trở đi). Nhưng khác các ví dụ trước (chỉ 1
device/dashboard), dự án này cần **5 device cùng gắn vào 1 Dashboard**:

- **Dùng flow "Thêm thiết bị" (Add Device) trên Mobile**, lặp lại 5 lần trên cùng 1 project/dashboard.
- **KHÔNG dùng Kit-Connect/quét QR** — flow đó luôn tạo Dashboard + Device MỚI mỗi lần connect, sẽ
  ra 5 Dashboard riêng biệt thay vì gộp vào 1.

Chi tiết đầy đủ: tài liệu nội bộ của IoTLabs
§3 và `07_HUONG_DAN_MOBILE.md`.

## Controls

Console/Mobile render trực tiếp từ Control Type Registry — xem `PROFILE_INTENT.json` từng thư mục
con. Bảng đầy đủ (typeKey/access/config từng Control, mapping Device Profile) ở
tài liệu nội bộ của IoTLabs.

AirQuality1 và AirQuality2 **dùng chung 1 Device Profile** ("Multi Devices — AirQuality") vì cấu
trúc Control giống hệt nhau — 4 Device Profile cho 5 Device.

## Automation — phạm vi V1

Automation liên-device (VD "AirQuality2 CO₂ cao → tự bật SmartPlug quạt") **không nằm trong phạm vi
V1** — AirQuality không có actuator, 2 board vật lý tách biệt, và engine automation hiện tại của
Device Registry chỉ hỗ trợ same-device. V1 dùng giải pháp thay thế: cảnh báo hiện trên Dashboard,
người dùng tự bật thiết bị khác qua cùng Dashboard. Chi tiết quyết định + thiết kế edge-automation
tham chiếu cho task riêng sau: `02_PRD_CONTROLS_VA_AUTOMATION.md` §5.

## Ghi chú kiểm thử

Chưa chạy thử trên phần cứng thật — xem
tài liệu nội bộ của IoTLabs cho danh sách test case cụ
thể, và `04_QA_TEST_PLAN.md` cho checklist đầy đủ theo từng tầng. Chủ dự án tự thực hiện QA trên
phần cứng thật và tự confirm trạng thái cuối cùng trước khi ví dụ này được coi là Verified.
