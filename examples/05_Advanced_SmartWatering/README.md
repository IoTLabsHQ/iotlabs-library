# Advanced — Smart Watering

Một dự án tưới cây tự động dựa trên ngưỡng độ ẩm đất (hysteresis), kết hợp
điều khiển tự động/thủ công (`controlTypeAutoOrManual`), và màn hình OLED
hiển thị trạng thái tại chỗ. Theo dõi và điều khiển hoàn toàn qua **IoTLabs
Console** (web) và **IoTLabs Maker mobile app** — cả hai đều render UI dựa
trên đúng Control Type Registry mà firmware báo cáo lên, không có màn hình
cấu hình riêng nào khác.

Thư viện Arduino Library Manager cần cài thêm: `DHT sensor library` (Adafruit),
`Adafruit GFX Library`, `Adafruit SSD1306`.

## Phần cứng (ESP32 Dev Kit V1)

| Linh kiện | GPIO | Ghi chú |
|---|---|---|
| OLED SDA / SCL | 21 / 22 | SSD1306 128x64, địa chỉ I2C 0x3C |
| Nút nhấn điều khiển bơm | 25 | `INPUT_PULLUP`, tích cực mức LOW, debounce 50ms |
| Relay (bơm) | 26 | tích cực mức HIGH — kiểm tra jumper Low/High Level Trigger trên module relay thật |
| DHT11 | 27 | nhiệt độ + độ ẩm không khí |
| Độ ẩm đất (analog) | 34 | ADC 12-bit, lấy trung bình 10 mẫu |
| Nút BOOT | 0 | giữ 10 giây để factory-reset (xem bên dưới) |

## Controls

Đây là toàn bộ Control mà firmware báo cáo lên Device Registry — cả IoTLabs
Console lẫn IoTLabs Maker mobile app đều render trực tiếp từ danh sách này,
không có control ẩn hay xử lý riêng theo platform.

| Control | typeKey | access | Hiển thị trên Console/Mobile |
|---|---|---|---|
| Temperature (Nhiệt độ) | `value` | read | Số liệu chỉ đọc trên dashboard |
| Air Humidity (Độ ẩm không khí) | `value` | read | Số liệu chỉ đọc trên dashboard |
| Soil Moisture (Độ ẩm đất) | `value` | read | Số liệu chỉ đọc trên dashboard |
| Pump (Auto/Manual) — `{"state":"on"\|"off","source":"auto"\|"manual"}` | `auto_or_manual` | read_write | Một công tắc bật/tắt duy nhất kèm badge nguồn điều khiển hiện tại (AUTO/MANUAL) |
| Soil ON Threshold (Ngưỡng bật) | `value` | read_write | Ô nhập số, chỉnh trực tiếp từ app |
| Soil OFF Threshold (Ngưỡng tắt) | `value` | read_write | Ô nhập số, chỉnh trực tiếp từ app |
| Manual Button (Nút nhấn thủ công, chỉ vật lý) | `button` | trigger | Sự kiện hiển thị trong lịch sử hoạt động của thiết bị |

Một quyết định thiết kế Control đáng chú ý:
- **Pump là 1 Control composite duy nhất, không phải 2 control riêng (mode
  + bơm).** `auto_or_manual` là một Control Type tự định nghĩa (không phải
  builtin của thư viện — Control Type Registry vốn được thiết kế mở để
  thêm type mới), báo cáo `{"state","source"}` như một object duy nhất và
  chỉ chấp nhận ghi `"state"`; thiết bị tự suy ra `"source"` — bất kỳ lệnh
  ghi `"state"` nào từ Console, Mobile, hay nút nhấn vật lý *chính là*
  thao tác ghi đè thủ công (manual override), và thiết bị tự động quay lại
  AUTO sau `MANUAL_OVERRIDE_MS` (15 phút) nếu không có thao tác nào mới.
  Nhờ vậy Console/Mobile chỉ cần render đúng 1 widget cho cái bơm, không
  phải 2 widget có quan hệ phụ thuộc lẫn nhau. Xem
  tài liệu nội bộ của IoTLabs
  để đọc PRD đầy đủ.

## Các use case thực tế đã xử lý trong code

Đây là những tình huống thực tế mà firmware hiện tại **đã implement và xử
lý được**, không phải ý tưởng dự định:

1. **Tưới tự động theo độ ẩm đất, không tưới thừa/thiếu ở ranh giới ngưỡng.**
   `applyAutoControl()` dùng hysteresis 2 ngưỡng (`soilOnThreshold` /
   `soilOffThreshold`): đất khô dưới ngưỡng ON → bật bơm; đủ ẩm từ ngưỡng
   OFF trở lên → tắt bơm; ở giữa hai ngưỡng giữ nguyên trạng thái hiện tại
   — chống việc relay đóng/cắt liên tục (chattering) khi độ ẩm dao động sát
   một ngưỡng duy nhất.
2. **Người dùng cần tưới thêm ngay lập tức từ xa (ví dụ vừa bón phân).**
   Ghi `{"state":"on"}` lên `control_4_auto_or_manual` từ Console/Mobile →
   `onPumpControl()` gọi `enterManualOverride(true)`, bơm bật ngay bất kể
   AUTO đang nói gì.
3. **Người dùng cần dừng tưới khẩn cấp (trời sắp mưa, phát hiện rò rỉ...)**
   và không muốn AUTO tự bật lại ngay sau đó. Ghi `{"state":"off"}` có hiệu
   lực ngay, và AUTO bị khoá trong `MANUAL_OVERRIDE_MS` (15 phút) —
   `applyAutoControl()` bị chặn bởi guard `pumpMode != PumpMode::AUTO`
   trong suốt khoảng đó.
4. **Người dùng đứng tại vườn, không có điện thoại trong tay.** Nhấn nút
   vật lý (GPIO25) gọi `triggerButtonAction()` → cùng đường xử lý
   `enterManualOverride()` với lệnh cloud, đảo trạng thái bơm hiện tại và
   báo cáo cả `control_4_auto_or_manual` lẫn sự kiện `control_7_button`
   lên cloud (nếu đang online) — hành vi giống hệt thao tác từ app.
5. **Người dùng thao tác thủ công rồi quên, có nguy cơ cây chết khô/úng.**
   `releaseManualOverrideIfExpired()` chạy mỗi vòng `loop()`, tự trả quyền
   về AUTO khi hết `MANUAL_OVERRIDE_MS` mà không có thao tác mới — không
   cần ai can thiệp lại.
6. **Lệnh cloud và nút nhấn vật lý gần như đồng thời.** Cả hai đường đều đi
   qua đúng 1 hàm `enterManualOverride()`/`reportPumpControl()` — không có
   2 nguồn sự thật khác nhau, tránh báo cáo sai lệch trạng thái thật.
7. **Chỉnh ngưỡng tưới theo loại cây/loại đất mà không nạp lại firmware.**
   `onThresholdOn()`/`onThresholdOff()` nhận ghi từ xa, có validate chéo
   (ngưỡng ON phải nhỏ hơn ngưỡng OFF) — ghi giá trị vi phạm sẽ bị từ chối
   và device echo lại giá trị cũ thay vì áp dụng giá trị sai.
8. **Mất điện/khởi động lại không làm mất ngưỡng đã chỉnh.**
   `soilOnThreshold`/`soilOffThreshold` được ghi vào NVS (`Preferences`),
   đọc lại đúng giá trị khi boot thay vì reset về mặc định.
9. **Ghi flash ngay trong callback lệnh MQTT từng gây brownout-reset thật
   trên phần cứng** (dòng tiêu thụ tăng đột biến khi vừa ghi NVS vừa xử lý
   radio giữa lúc ack gói MQTT). Xử lý bằng cờ `soilThresholdsDirty`, hoãn
   ghi NVS thật sự sang vòng `loop()` thay vì ghi ngay trong callback.
10. **Mất kết nối cloud không làm thiết bị "đứng hình".** Nút nhấn vật lý,
    hysteresis AUTO, và cả timeout `MANUAL_OVERRIDE_MS` đều chạy hoàn toàn
    cục bộ dựa trên `millis()` — không phụ thuộc việc có nhận được lệnh mới
    từ MQTT hay không.
11. **Người dùng cuối không có app REST client để provisioning WiFi qua
    điện thoại.** `provisioning_page.h` thêm form HTML thật ở cổng 8080,
    proxy sang `/provision` của thư viện — người dùng chỉ cần mở trình
    duyệt trên điện thoại, không cần Postman hay công cụ kỹ thuật.
12. **Cần factory-reset khi đổi chủ thiết bị/đổi mạng WiFi mà không có màn
    hình cấu hình.** Giữ nút BOOT (GPIO0) đủ 10 giây → `updateBootButton()`
    gọi `iot.factoryReset()`, xoá cấu hình và quay lại chế độ provisioning.

## Ghi chú thiết kế: những gì ví dụ này tự thêm ngoài core của thư viện

1. **Không lưu trạng thái ứng dụng qua reboot.** `pumpMode`/ngưỡng/`pumpOn`
   chỉ nằm trong RAM và reset về mặc định mỗi lần boot (bơm luôn về OFF,
   AUTO — an toàn theo thiết kế). Chỉ có identity WiFi/activation là được
   lưu bền (do chính IoTLabs Library xử lý, không phải code ứng dụng).
2. **Giữ nút để factory-reset là code ứng dụng, không phải tính năng có
   sẵn của thư viện.** `iot.factoryReset()` tồn tại, nhưng IoTLabs Library
   không tự có cơ chế đếm thời gian giữ nút (đã kiểm tra: không có logic
   `holdMs`/long-press nào trong `src/` hay các ví dụ khác). Ví dụ này tự
   implement toàn bộ phần đếm giữ 10 giây trong `updateBootButton()`.
3. **Form HTML cho provisioning cũng là code ứng dụng.** SoftAP của
   IoTLabs Library (`Esp32SoftApProvisioningTransport`) chỉ có REST JSON
   thô — `/device-info`, `/wifi/networks`, `/provision` — không có HTML.
   Đã xử lý riêng cho ví dụ này (không phải thay đổi thư viện) bằng
   `provisioning_page.h` — xem bên dưới.

## Trải nghiệm provisioning: trước và sau `provisioning_page.h`

| | Mặc định thư viện (không có `provisioning_page.h`) | Ví dụ này (có) |
|---|---|---|
| Trải nghiệm trên điện thoại | Cần app REST client (Postman...) để tự dựng JSON POST `/provision` | Form trình duyệt thuần tại `http://192.168.4.1:8080/` — điền SSID/mật khẩu/token, bấm submit |
| `/device-info` | JSON thô trên trình duyệt | Không đổi — vẫn JSON thô, chỉ `/provision` có form |
| Nằm ở đâu | N/A | Code ứng dụng riêng của ví dụ này, **không** phải tính năng thư viện. Dự án khác muốn UX này phải tự thêm lại |
| Chi phí thêm | không có | thêm 1 instance `WebServer` (cổng 8080) + 1 lượt proxy loopback qua `HTTPClient` mỗi lần submit |
| Vì sao không thêm route vào server cổng 80 sẵn có của thư viện | — | `WebServer` của nó là thành viên private của `Esp32SoftApProvisioningTransport`, không có API đăng ký route cho code ứng dụng |

## Ghi chú kiểm thử

Chạy thật ngày 2026-08-12: ESP32 Dev Kit V1 thật (đủ ngoại vi), Console UI
thật (điều khiển bằng Playwright), Device Registry local thật, broker MQTT
thật đang host (`mqtt.vn1.iotlabs.vn:8883`), điện thoại thật join SoftAP của
thiết bị và submit form của `provisioning_page.h`. Các phát hiện, xếp từ bất
ngờ nhất đến ít bất ngờ nhất:

1. **`Esp32ArduinoHttpClient.cpp` luôn dùng `WiFiClientSecure` — không có
   đường plain-HTTP nào cả.** `tlsInsecure` (config nâng cao qua
   `begin(config)`, không còn là macro sketch) chỉ bỏ qua việc
   *xác thực* certificate; nó không bao giờ tắt hẳn TLS. Một Device
   Platform local chạy plain HTTP (setup thông thường `pnpm run start:dev`)
   **không thể kết nối được từ firmware thật** như hiện trạng. Đã workaround
   cho lần test này bằng một relay TLS-terminating local dùng một lần
   (self-signed cert ở `:3443` forward sang `:3100` — một pipe byte
   `tls`↔`net` trần, không quan tâm giao thức, ~30 dòng Node) —
   `tlsInsecure=true` chấp nhận cert self-signed vì không bao giờ kiểm tra
   nó. Ai làm test phần cứng thật với backend local bằng thư viện này cũng
   cần đúng workaround này, hoặc một backend local có TLS thật.
2. **MQTT subscriber runtime của Device Registry cần credential broker
   riêng, tách khỏi credential per-device dùng cho activation.**
   `MQTT_RUNTIME_USERNAME`/`PASSWORD` (một tài khoản đặc quyền duy nhất,
   subscribe wildcard `devices/+/telemetry`/`/status`/`/ping` trên mọi
   thiết bị) mặc định rỗng và khác với `mqttClientId`/username/password
   riêng từng thiết bị mà `ActivationService` tự cấp (phần này đã hoạt
   động và đã được lưu bền — xác nhận qua `GET /api/v1/devices/:id`, xem
   bên dưới). Không có credential runtime, Device Registry không quan sát
   được traffic MQTT của *bất kỳ* thiết bị nào, nên Console/Mobile không
   bao giờ thấy `status: online` hay telemetry sống — dù kết nối MQTT của
   chính thiết bị hoàn toàn thật và đang hoạt động. Lần test này không có
   credential đó, nên activation được xác nhận trực tiếp qua record của
   Device Registry thay vì qua trường status trên Console UI:
   ```json
   {
     "hardwareId": "400392F57630",
     "mqttClientId": "iotlabs_3a82a3539941",
     "provisioningState": "provisioned",
     "status": "inactive",        // sẽ là "online" nếu có cấu hình credential runtime
     "activatedAt": "2026-08-12T16:38:02.246Z"
   }
   ```
   và qua Serial log của chính thiết bị, vẫn liên tục bắn `[EVENT] ...
   PUBLISHED` mỗi chu kỳ cảm biến ~2s rất lâu sau khi activate — bằng
   chứng thiết bị tự nó vẫn kết nối và publish thật, độc lập với Console.

   **Follow-up 2026-08-13 — thử dùng luôn credential riêng của thiết bị này
   như một đường tắt, không work.** Username/password riêng của thiết bị
   *có thể* lấy được (chỉ là không lấy từ Device Registry, nơi cố tình
   không bao giờ lưu nó — xem comment trong `audit-event.schema.ts`) — MQTT
   Service, nơi thực sự cấp nó, vẫn giữ một bản sao, `GET
   /connections/:projectId` trên đó trả về `{username, password, clientId,
   ...}` dạng plaintext. Đã thử giá trị đó trực tiếp làm
   `MQTT_RUNTIME_USERNAME`/`PASSWORD` (dùng `MQTT_RUNTIME_CLIENT_ID` khác
   với client id thật của thiết bị, cố ý — trùng client id sẽ đá kết nối
   thật của thiết bị ra khỏi broker). Kết quả: authenticate thành công (log
   `Runtime MQTT connected` mỗi ~2s), nhưng không bao giờ có telemetry nào
   đến (`GET /devices/:id/state` giữ nguyên `{reported: {}}` suốt), và log
   connect cứ lặp lại — broker gần như chắc chắn chỉ authorize credential
   này cho đúng topic của thiết bị đó, âm thầm drop kết nối ngay sau lệnh
   `SUBSCRIBE` wildcard `devices/+/telemetry`, và `mqtt.js` tự reconnect
   (`reconnectPeriod` 2s) vào đúng lỗi đó lặp lại — `MqttRuntimeClient`
   không log sự kiện `close`/`reconnect`, chỉ log `connect`, nên nhìn ban
   đầu tưởng khả quan hơn thực tế. Fix thật cần một tài khoản runtime có
   đặc quyền thật trên broker, không phải credential của riêng một thiết bị.

   **2026-08-13, cuối ngày hôm đó — fix thật, xuất phát từ chính cách đặt
   vấn đề của user: "thực tế người dùng chỉ bao giờ được xem/điều khiển
   đúng thiết bị của họ."** Điều đó hoàn toàn đúng, và nó định hình lại
   toàn bộ vấn đề: thiết kế một subscriber wildcard toàn cục duy nhất chưa
   bao giờ khớp với mô hình ACL thật của broker này, vốn đã theo từng
   thiết bị. Fix thật (nằm trong `iotlabs-device-registry`, không phải repo
   này): `MqttRuntimeClient` có thêm `connectDevice(identity)` — một kết
   nối riêng cho từng thiết bị, chỉ subscribe đúng
   `devices/{clientId}/telemetry|status|ping` của thiết bị đó, dùng đúng
   credential riêng thiết bị mà ACL đã cho phép. `RuntimeController` mở kết
   nối này lazy (khi vào trang chi tiết thiết bị / gửi lệnh — "get mqtt qua
   mqtt service by projectId, dùng mqtt đó điều khiển thiết bị và
   subscribe topic status" là spec nguyên văn), `ActivationService` cũng mở
   eager ngay sau khi activate, và `main.ts` mở lại cho mọi thiết bị đã
   provisioned mỗi lần boot (trạng thái của `connectDevice()` chỉ ở
   in-memory, không sống sót qua restart). Đã xác nhận hoạt động thật với
   đúng thiết bị này:
   ```json
   {"status": "online", "reported": {"control_3_value": 27, "uptime_ms": 24376160}}
   ```
   và một lệnh `POST /commands` thật
   (`{"target":"control_5_on_off","command":"set","value":"on"}`) trả về
   `{"accepted": true}` và giữ nguyên `status: "online"` — cả hai đều từng
   là `502 MQTT_PUBLISH_FAILED` / `status: "inactive"` trước fix này.

   **Regression thật bắt được và fix ngay trong lúc làm**: bản đầu tiên gọi
   `connectDevice()` vô điều kiện từ `ActivationService`/`RuntimeController`
   — vì e2e test boot đúng app thật và đi qua đúng những code path này
   (`POST /activate`, `GET /state`), nó thử kết nối broker thật bằng
   credential giả của MQTT identity service giả trong e2e, làm crash Jest
   lúc teardown (timer reconnect của `mqtt.js` vẫn bắn sau khi môi trường
   test đã tắt). Đã fix bằng cách thêm `MQTT_RUNTIME_AUTO_CONNECT` (mặc
   định false) chặn mọi điểm gọi connect thật, ép false trong
   `test/setup-env.ts` — khớp với đúng invariant đã có sẵn của codebase này
   ("kết nối MQTT là tường minh, không bao giờ tự động", comment gốc trong
   `main.ts`) mà bản đầu tiên đã vi phạm khi nối các lệnh gọi mới vào code
   xử lý request bình thường mà thiếu đúng guard đó. Cả 87 e2e + 58 unit
   test đều pass sau fix (chúng không pass ngay sau khi bản đầu tiên lên).
3. **Console UI xử lý đúng Control kiểu `select` và `value` read_write —
   không phát hiện bug nào.** `ControlIntentBuilder` thực sự generic dựa
   trên `GET /api/v1/control-types`, không hardcode theo từng type; cả 8
   control (bao gồm 2 control kiểu mới) đều tạo và render đúng trên trang
   Profile detail. Cái thiếu coverage thật ra là *test helper*
   (`tests/e2e/page-objects/ProfileFormPage.ts`'s `addControl()`) — nó
   không có cách override `access` của control khác với `defaultAccess`
   của type, nên trước đây không thể nào tạo được control `value` kiểu
   read_write. Đã mở rộng thêm tham số `access` tuỳ chọn (tương thích
   ngược, các lệnh gọi cũ không bị ảnh hưởng).
4. **Tài khoản e2e Maker cố định (`maker@iotlabs.vn`) bị `emailVerified`
   chưa set trong Mongo `iotlabs_maker` local trên máy này**, chặn hoàn
   toàn đăng nhập Console ("Vui lòng xác thực email trước khi đăng nhập").
   Đã fix bằng `db.users.updateOne(..., {$set: {emailVerified: true}})`
   trực tiếp — đúng pattern mà fixture e2e của chính `iotlabs-maker` đã
   dùng để seed tài khoản này, không phải workaround mới.
5. **Board này không gặp gotcha kiểu `CDCOnBoot` trên Serial**, khác với
   board ESP32-S3-Zero/ESP32-C3-SuperMini trong phiên board-rig trước (cần
   USB native, `CDCOnBoot=cdc`). Dev Kit V1 này dùng cầu USB-serial kiểu
   CP2102/CH340 cổ điển với auto-reset thật qua RTS — `Serial` hoạt động
   ngay với FQBN thuần `esp32:esp32:esp32`, không cần thêm option board nào.
6. Các artifact thật do Console tạo ra trong lần chạy này (DB dev local,
   không có ý nghĩa ngoài máy này — không phải giá trị `#define` mặc định
   ship trong ví dụ ở trên): profile `PROF_314261e0` ("Smart Watering
   1786551911412"), device `DEV_cdfa929d`, project `esp32-blink`.
