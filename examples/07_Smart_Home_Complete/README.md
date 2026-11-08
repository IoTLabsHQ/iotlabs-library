# Advanced — Smart Home Complete

**Trạng thái: DRAFT — compile PASS thật, chưa chạy trên phần cứng thật.** Firmware đã viết và
compile thành công (xem "Build" bên dưới), nhưng chưa có run thật trên board + Console/Mobile —
chủ dự án sẽ tự thực hiện QA trên phần cứng thật và confirm trạng thái cuối cùng, xem
tài liệu nội bộ của IoTLabs.

Một firmware duy nhất, flash giống hệt nhau lên mỗi board triển khai cho dự án này. Mỗi board là
1 Device **hoàn toàn độc lập** trên Device Registry — không gateway, không phụ thuộc board khác.
Khác bản thiết kế trước, **không còn "role" chọn qua provisioning** — mỗi board luôn có đúng 1 bộ
năng lực cố định:

- **Relay** — bật/tắt 1 tải (đèn, quạt, hoặc bất kỳ thiết bị nào đấu qua relay).
- **DHT11** — nhiệt độ + độ ẩm không khí tại chỗ.
- **LED RGB** — trạng thái thiết bị hiển thị tại chỗ (WS2812 onboard).

Muốn thêm cảm biến/actuator khác sau này: thêm 1 hằng GPIO + 1 cặp `report()`/`on()` trong `.ino`
và 1 control tương ứng trong `PROFILE_INTENT.json` — không cần thêm "role" mới hay nhánh rẽ runtime.

Nhiều board deployed độc lập được gộp **hiển thị** (không gộp vận hành) trên 1 Dashboard ở Console.

Thư viện Arduino Library Manager cần cài thêm: `DHT sensor library` (Adafruit), `Adafruit NeoPixel`.

## Hành trình từ 0: IoTLabs Console → thiết bị chạy thật

Làm đúng thứ tự dưới đây — mỗi bước dùng đúng tên nút/nhãn thật trên IoTLabs Console, không cần biết trước gì về hệ thống.

1. **Đăng nhập IoTLabs Console**, bấm **Tạo Dự án** để tạo 1 dự án mới cho project này.
2. Trong dự án, ở mục "Device Profile" bấm **Tạo Device Profile** — điền **Tên Device Profile** (ví dụ "ESP32 Smart Home Node"), **Firmware family** để trống cũng được, rồi Lưu. Chỉ cần đúng **1 Device Profile** cho mọi board — mọi board flash cùng firmware này đều dùng chung 1 profile.
3. Bấm **Thiết kế** trên card Device Profile vừa tạo để vào trang thiết kế tính năng, rồi bấm **+ Thêm Control** 4 lần, thêm đúng 4 Control theo đúng bảng ở mục "Controls" bên dưới (Bật/tắt tải, Nút vật lý, Nhiệt độ, Độ ẩm). Với 2 Control kiểu "Giá trị" (Nhiệt độ, Độ ẩm), có thể bấm nút **Chọn từ danh sách chuẩn** cạnh ô đơn vị để tự điền sẵn đơn vị + khoảng giá trị hợp lý thay vì gõ tay.
4. Quay lại trang dự án, ở card Device Profile này bấm **Tạo bảng điều khiển** — đặt tên (ví dụ "Smart Home Dashboard"), Lưu. Dashboard này sẽ hiển thị mọi board dùng chung Device Profile.
5. Vào mục **Thiết bị** ở menu bên trái, bấm **Thêm thiết bị** — chọn đúng Dashboard vừa tạo ở bước 4, đặt tên riêng cho từng board thật (ví dụ "Đèn phòng khách"), Tạo thiết bị. Lặp lại bước này cho mỗi board bạn sẽ triển khai.
6. Mở thiết bị vừa tạo, bấm **Bắt đầu cấp quyền** — Console hiện ra **Mã kích hoạt** (chỉ hiển thị đúng 1 lần, dùng 1 lần, có hạn sử dụng) — bấm **Sao chép mã** và giữ lại, sẽ cần nhập ở bước 8.
7. Build + nạp firmware này lên board thật — xem mục "Build" ngay bên dưới.
8. Cấp nguồn board — đèn LED nhấp nháy **vàng** (chế độ chờ cài đặt). Kết nối WiFi điện thoại/máy tính vào mạng riêng của board (`IoTLabs-XXXXXX`), mở trình duyệt tới `http://192.168.4.1/` — xem chi tiết ở mục "Provisioning" bên dưới — nhập WiFi nhà + đúng Mã kích hoạt vừa sao chép ở bước 6, bấm Provision.
9. Board tự khởi động lại, đèn chuyển xanh dương (đang kết nối) rồi xanh lá (thành công). Mở lại Console/Mobile — thiết bị hiện **Online**, mở Dashboard đã tạo ở bước 4 để xem/điều khiển thật.

Lặp lại bước 5-6 và 8-9 cho từng board tiếp theo — bước 1-4 chỉ làm 1 lần cho cả dự án.

## Build

```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> examples/07_Smart_Home_Complete
arduino-cli upload  --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --library <repo-root> -p <PORT> examples/07_Smart_Home_Complete
```

`CDCOnBoot=cdc` **bắt buộc** trên ESP32-C3 Supermini — thiếu thì sketch vẫn chạy nhưng `Serial`
không bao giờ phản hồi qua cổng USB (gotcha đã xác nhận thật trên phần cứng, xem
tài liệu nội bộ của IoTLabs).

Compile PASS thật (2026-08-18): flash 1.197.635/1.310.720 byte (91%), RAM 37.860/327.680 byte
(11%). Chưa chạy thật trên phần cứng — xem mục "Ghi chú kiểm thử" cuối file.

## Phần cứng (ESP32-C3 Supermini)

GPIO cố định (hardcode), giống hệt nhau trên mọi board — cùng 1 firmware, cùng 1 sơ đồ nối dây:

| Chức năng | GPIO | Ghi chú |
|---|---|---|
| Relay control | 4 | Active HIGH — kiểm tra jumper Low/High Level Trigger trên module relay thật |
| DHT11 data | 5 | |
| LED RGB (WS2812 onboard) | 8 | Cần xác minh với board thật đang dùng — layout LED onboard khác nhau tuỳ nhà sản xuất clone |
| Nút BOOT onboard | 9 | Giữ 10s = factory reset; nhấn ngắn = đảo trạng thái relay |
| UART0 (USB-Serial) | 20 (RX) / 21 (TX) | Nạp code/debug log |

## Provisioning — dùng trang setup có sẵn của thư viện

Ví dụ này **không** còn tự viết form HTML riêng (`provisioning_page.h` đã bị xoá). Trang setup
SoftAP (nhập WiFi + activation token) giờ là tính năng **có sẵn trong IoTLabs Library**
(`Esp32SoftApProvisioningTransport`) — mọi ví dụ đều có miễn phí, không cần code thêm gì. Tiêu đề
trang có thể đổi bằng 1 dòng `#define` trong `.ino`:

```cpp
#define IOTLABS_PROJECT_NAME "Smart Home Complete Setup"  // tuỳ chọn — bỏ dòng này dùng mặc định
#include <IoTLabs.h>
```

Không định nghĩa `IOTLABS_PROJECT_NAME` → trang setup dùng tiêu đề mặc định **"IoTLabs Device
Setup"**. Truy cập `http://192.168.4.1/` (cổng 80, không còn cổng 8080 riêng như bản trước) trong
lúc board ở chế độ provisioning để mở trang setup.

Từ nay `IOTLABS_DEVICE_ID` (Device id thật copy từ Console, dạng `DEV_xxxxxxxx`) là **bắt buộc**
khai báo bằng `#define` trước `#include <IoTLabs.h>` cho mọi sketch dùng `iot.begin()` — thiếu hoặc
rỗng thì `begin()` thất bại, `lastError()` trả về `"DEVICE_ID_REQUIRED"`. Giá trị này còn quyết định
SSID mà AP provisioning phát ra (`IoTLabs-<6 ký tự cuối Device id>`), nên Console có thể hiện sẵn
đúng tên mạng cần join trước cả khi nạp firmware. Có thể `#define IOTLABS_DEVICE_ACTIVATION_SECRET
"<mã kích hoạt>"` (tuỳ chọn) để trang setup tự điền sẵn Mã kích hoạt, khỏi phải gõ tay mỗi lần
reflash lúc đang phát triển/test.

## Controls

Console/Mobile render trực tiếp từ Control Type Registry, không có control ẩn hay xử lý riêng theo
platform — xem `PROFILE_INTENT.json`:

| Control | typeKey | access | Hiển thị trên Console/Mobile |
|---|---|---|---|
| Bật/tắt tải | `on_off` | read_write | Công tắc bật/tắt |
| Nút vật lý | `button` | trigger | Sự kiện trong lịch sử hoạt động |
| Nhiệt độ | `value` | read | Số liệu chỉ đọc |
| Độ ẩm | `value` | read | Số liệu chỉ đọc |

Chỉ 1 Device Profile duy nhất cho mọi board — không còn 2 profile (relay/climate) như bản thiết kế
trước, vì mọi board giờ luôn có đủ cả 2 năng lực.

## LED RGB — trạng thái

| Trạng thái | Màu | Kiểu |
|---|---|---|
| Khởi động / kết nối WiFi | Xanh dương | Nhấp nháy |
| Chế độ provisioning | Vàng | Nhấp nháy |
| Online (đã kết nối cloud) | Xanh lá | Sáng liên tục |
| Mất kết nối cloud / lỗi đọc DHT11 kéo dài | Đỏ | Nhấp nháy |
| Relay đang ON | Chớp trắng đè lên màu nền | Chớp 1 lần mỗi 3 giây |

## Vì sao bỏ "role" chọn qua provisioning

Bản thiết kế trước dùng 1 firmware nhưng 2 "role" khác nhau (relay-only / climate-only), chọn lúc
provisioning và lưu vào NVS riêng của app — kéo theo 2 Device Profile, `IOTLABS_PROFILE_ID` phải
gán runtime thay vì `#define`, và form provisioning phải tự thêm field `role`/`label` (thư viện
không hỗ trợ field tuỳ ý qua `/provision`). Bản hiện tại đơn giản hoá: mỗi board luôn có đủ cả
relay lẫn DHT11, không còn phân nhánh — 1 Device Profile, `IOTLABS_PROFILE_ID` dùng lại đúng
`#define` chuẩn như mọi ví dụ khác, và không cần form/NVS riêng nào cho role/label nữa.

## Ghi chú kiểm thử

Chưa chạy thử trên phần cứng thật — xem
tài liệu nội bộ của IoTLabs cho danh sách test case cụ
thể, và `04_QA_TEST_PLAN.md` cho checklist đầy đủ theo từng tầng. Chủ dự án tự thực hiện QA trên
phần cứng thật và tự confirm trạng thái cuối cùng trước khi ví dụ này được coi là Verified.
