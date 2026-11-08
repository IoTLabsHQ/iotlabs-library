# Advanced 10 — Priority Load Shedding Controller

**Trạng thái: DRAFT — compile PASS thật, chưa chạy trên phần cứng thật.** Firmware đã viết và
compile thành công (xem "Build" bên dưới), nhưng chưa có run thật trên board + PC817/ACS712/relay
thật — chủ dự án sẽ tự thực hiện QA trên phần cứng thật và confirm trạng thái cuối cùng, xem
tài liệu nội bộ của IoTLabs và
`08_QA_TEST_CASES.md`.

Bộ điều khiển tự động ưu tiên cắt tải khi mất điện lưới: phát hiện mất điện lưới 220V AC qua cách
ly quang PC817, tự động cắt tải theo thứ tự ưu tiên (không thiết yếu trước) để bảo vệ nguồn dự
phòng (UPS/máy phát), giám sát dòng điện dự phòng qua ACS712, và khôi phục tải khi lưới ổn định
trở lại. Nguồn ý tưởng:
https://maker.iotlabs.vn/projects/bo-dieu-khien-tu-dong-uu-tien-tai-khi-mat-dien-luoi

## CẢNH BÁO AN TOÀN — ĐỌC TRƯỚC KHI ĐẤU DÂY (mạch cảm biến chạm điện lưới 220V AC)

- **Luôn lắp RCD/aptomat (CB chống giật)** phía trước bất kỳ mạch 220V AC nào trước khi thao tác.
- **KHÔNG BAO GIỜ** nối trực tiếp GPIO của ESP32 vào mạch điện AC. **PC817 là lớp cách ly an toàn
  DUY NHẤT** giữa điện lưới và board này — **không được bỏ qua/thay thế** bằng chia áp điện trở
  trực tiếp vào AC.
- **Toàn bộ phần đấu dây phía AC của relay (tiếp điểm relay đóng/cắt tải AC thật) PHẢI do thợ
  điện có chuyên môn thực hiện.**
- **Firmware này chỉ triển khai phần tín hiệu điều khiển điện áp thấp** (GPIO ESP32 ↔ ngõ ra
  PC817, GPIO ESP32 ↔ chân điều khiển module relay) — **không triển khai phần đấu dây AC**, xem
  đây là ngoài phạm vi (out of scope), chỉ tài liệu hoá yêu cầu.

## Build

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --library <repo-root> examples/10_Priority_Load_Shedding_Controller
arduino-cli upload  --fqbn esp32:esp32:esp32 --library <repo-root> -p <PORT> examples/10_Priority_Load_Shedding_Controller
```

Board: **ESP32 DevKit V4** (ESP32 cổ điển, không phải C3) — không có gotcha `CDCOnBoot`.

Compile PASS thật (2026-08-18):

| | Flash | RAM |
|---|---|---|
| `10_Priority_Load_Shedding_Controller` | 1.162.652/1.310.720 byte (88%) | 48.616/327.680 byte (14%) |

Không cần cài thêm thư viện Arduino Library Manager nào — chỉ dùng `digitalRead`/`analogRead`/
`digitalWrite` chuẩn.

## Phần cứng / GPIO (ESP32 DevKit V4)

| Thành phần | Chân ESP32 | Ghi chú |
|---|---|---|
| PC817 collector (cảm biến mất lưới) | GPIO35 | Input-only, **không** có pull nội bộ — xem gotcha #1 |
| PC817 emitter | GND | |
| Relay IN1 — Essential (Thiết yếu) | GPIO25 | OUTPUT, không bao giờ tự động cắt |
| Relay IN2 — Important (Quan trọng) | GPIO26 | OUTPUT, cắt thứ 2 |
| Relay IN3 — Optional (Tuỳ chọn) | GPIO27 | OUTPUT, cắt thứ 1 |
| Relay IN4 — Non-essential (Không thiết yếu) | GPIO14 | OUTPUT, cắt ngay khi mất lưới |
| ACS712 output (qua mạch chia áp) | GPIO34 | Input-only ADC1 — **bắt buộc** có chia áp, xem gotcha #2 |
| Buzzer signal | GPIO32 | OUTPUT |

### Gotcha #1 — GPIO34/GPIO35 không có pull-up/pull-down nội bộ

Đây là 2 chân input-only của ESP32. `pinMode(GRID_PIN, INPUT)` là mode hợp lệ duy nhất —
`INPUT_PULLUP` **không có tác dụng** trên 2 chân này (không báo lỗi, chỉ âm thầm bị bỏ qua). Điện
trở pull-up lên 3V3 cho PC817 collector **bắt buộc phải là điện trở ngoài** gắn trên board — thiếu
nó, GPIO35 sẽ trôi nổi (floating) và mọi giá trị đọc được đều vô nghĩa.

### Gotcha #2 — ACS712 chạy 5V, bias giữa nguồn vượt quá ADC max 3.3V của ESP32

ACS712 chạy nguồn 5V, ngõ ra analog được phân cực (bias) quanh giữa nguồn (~2.5V ở dòng điện 0A).
Mức bias 2.5V đó — và bất kỳ dao động dương nào phía trên nó — **vượt quá điện áp ADC max 3.3V**
của ESP32. Dây nối **bắt buộc phải có mạch chia áp** (voltage divider) — hoặc dùng cảm biến dòng
điện 3.3V-native khác — giữa ACS712 Vout và GPIO34. Đây là **bước đấu dây bắt buộc**, không chỉ là
hằng số firmware — bỏ qua bước này có nguy cơ **làm hỏng chân ADC của ESP32**. Hằng số
`ACS712_DIVIDER_RATIO` trong `.ino` là **giá trị placeholder**, phải hiệu chỉnh theo đúng cặp điện
trở thật đã lắp (cùng pattern với `BATTERY_DIVIDER_RATIO` của ví dụ 09).

### Giả định cực tính relay module

Firmware viết cho module relay 5V phổ biến kiểu **active-LOW** (chân IN = LOW → relay đóng/ON,
HIGH → relay mở/OFF) — kiểu phổ biến nhất ở các module relay 4 kênh giá rẻ. Nếu module thật là
active-HIGH, đổi hằng số `RELAY_ACTIVE_LEVEL` trong `.ino`. Chi tiết:
tài liệu nội bộ của IoTLabs.

## Controls

Console/Mobile render trực tiếp từ Control Type Registry — xem `PROFILE_INTENT.json`. Bảng đầy đủ
(typeKey/access/config từng Control, state machine cắt tải theo mức ưu tiên) ở
tài liệu nội bộ của IoTLabs.

| Control ID | typeKey | access | config | Mô tả |
|---|---|---|---|---|
| `control_1_on_off` | `on_off` | `read` | — | `grid_lost` — trạng thái mất điện lưới (đã debounce) |
| `control_2_on_off` | `on_off` | `read_write` | — | `relay_essential` — không bao giờ tự động cắt |
| `control_3_on_off` | `on_off` | `read_write` | — | `relay_important` — cắt thứ 2 khi quá dòng |
| `control_4_on_off` | `on_off` | `read_write` | — | `relay_optional` — cắt thứ 1 khi quá dòng |
| `control_5_on_off` | `on_off` | `read_write` | — | `relay_nonessential` — cắt ngay khi mất lưới |
| `control_6_value` | `value` | `read` | `{"unit":"A"}` | `backup_current` — dòng điện dự phòng thật (ACS712) |
| `control_7_value` | `value` | `read_write` | `{"unit":"A","min":1,"max":15}` | `current_limit` — ngưỡng dòng, mặc định 8.0A (là setpoint số, **không** render gauge dù có min/max vì `access=read_write`) |
| `control_8_on_off` | `on_off` | `read` | — | `buzzer_alarm` — bật liên tục khi `OVERCURRENT_CRITICAL` |
| `control_9_text` | `text` | `read` | — | `system_status` — `NORMAL` / `GRID_LOST` / `GRID_LOST_SHEDDING` / `OVERCURRENT_CRITICAL` |

## Automation — state machine cắt tải theo mức ưu tiên

Toàn bộ logic viết tay bằng `millis()` non-blocking ngay trong `.ino` (không dùng
`AutomationEngine` của thư viện — trigger của engine cần giá trị số, không phải boolean, và chưa
ví dụ nào trong thư viện dùng engine này; giữ đúng convention hand-written control logic của repo).

- **Debounce mất lưới**: lấy mẫu GPIO35 mỗi ~20ms; chỉ xác nhận chuyển trạng thái khi ≥90% mẫu
  trong cửa sổ trượt ~400ms đồng thuận trạng thái mới (mạch opto tham chiếu điện lưới thường dao
  động ở tần số lưới điện 50/60Hz khi còn điện, chỉ ổn định khi thật sự mất điện — đọc 1 mẫu đơn lẻ
  sẽ false-trigger). **Thời gian debounce cần được validate trên phần cứng thật**, xem
  `04_QA_TEST_PLAN.md`.
- Mất lưới xác nhận → cắt ngay `relay_nonessential`. Essential/Important/Optional vẫn bật (chạy
  bằng nguồn dự phòng). `system_status = GRID_LOST`.
- Trong lúc mất lưới, mỗi 2 giây (settle delay): nếu `backup_current > current_limit` → cắt tiếp
  tầng thấp nhất còn hoạt động (Optional → Important, **không bao giờ cắt Essential**).
  `system_status = GRID_LOST_SHEDDING`.
- Nếu `backup_current` vẫn vượt `current_limit` khi chỉ còn Essential → `buzzer_alarm = true` liên
  tục, `system_status = OVERCURRENT_CRITICAL` — cần con người can thiệp, firmware không tự cắt
  Essential.
- Lưới phục hồi xác nhận → khôi phục relay theo thứ tự ngược (Important → Optional → Nonessential)
  với stagger delay 750ms giữa các bước để tránh dòng khởi động (inrush) cộng dồn; xoá
  `buzzer_alarm`; `system_status = NORMAL`.
- Ghi trực tiếp (manual write) vào `relay_important`/`relay_optional`/`relay_nonessential` qua
  Console/Mobile được áp dụng ngay (phục vụ test/ghi đè thủ công), nhưng state machine sẽ **cắt lại
  ngay trong `loop()` kế tiếp** nếu tầng đó đang trong phạm vi bị cắt và lưới vẫn đang mất — state
  machine luôn thắng ghi đè thủ công còn hiệu lực trong 1 sự kiện cắt tải đang diễn ra.
- Mọi control được report khi đổi trạng thái, không chỉ khi có write.

## Ghi chú kiểm thử

Chưa chạy thử trên phần cứng thật — chỉ mới compile PASS. Đặc biệt **thời gian debounce mất lưới
(~400ms, ngưỡng 90%) chưa được validate với mạch PC817/điện lưới thật** — cần đo trên phần cứng
thật trước khi tin cậy con số này. Xem
tài liệu nội bộ của IoTLabs cho danh sách
test case cụ thể, và `04_QA_TEST_PLAN.md` cho checklist đầy đủ theo từng tầng. Chủ dự án tự thực
hiện QA trên phần cứng thật (kèm tải giả lập an toàn) và tự confirm trạng thái cuối cùng trước khi
ví dụ này được coi là Verified.
