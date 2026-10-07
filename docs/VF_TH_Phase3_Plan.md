# Giai đoạn 3 — Kế hoạch: `TH_Data` + `TH_Status`, chỉ silent loopback

**Trạng thái: ĐÃ DUYỆT 2026-10-07** — cả hai câu hỏi ở mục 10 theo **phương án A** (T/RH = 0/0 trước lần đọc đầu; `Counter` theo khe 1 s).

Căn cứ: DBC v1.5 (`BO_ 1024` `TH_Data_01`, `BO_ 1026` `TH_Status_01` và các `CM_`), proposal §5.1–5.2, ID Map v1.3 §2.1; kết quả giai đoạn 2 (`app_meas` đã có tuổi, cờ, bộ đếm); cách module góc làm giai đoạn 5 (`VF_Angle_Module/docs/VF_Angle_Phase5_Plan.md`) — chép cách làm, không chép code.

---

## 0. Mục tiêu — kiểm được trên board

1. **`TH_Data` và `TH_Status` mỗi 1000 ms** đi qua bxCAN thật (bit timing, mailbox, lập lịch) và **quay về nguyên vẹn từng byte** qua silent loopback.
2. **Không một bit nào lên dây**, vì hai lý do độc lập:
   - bxCAN ở **silent loopback**: CANTX giữ recessive, không ACK ai;
   - `EN_CAN1` (PA10) vẫn HIGH, transceiver standby — và **vẫn không có hàm nào** đưa nó sang Normal (CLAUDE.md §7.1).
3. **Cổng `node_id`**: `node_id` không hợp lệ ⇒ không dựng frame, không chạm mailbox, một bộ đếm nói ra điều đó.
4. **Layout khớp DBC v1.5**, chứng minh bằng byte viết tay từ DBC **và** bằng một test đọc thẳng file DBC trong `contracts/` (mục 6).
5. **Frame mang đúng tuổi**: rút cảm biến thì `DataAge` trong byte 4–5 của frame tăng và `Flags` bật STALE — thấy được ngay trong byte thô.

Phát thật lên dây là **giai đoạn 4**, cùng lúc với kho config có CRC.

---

## 1. Dữ kiện đã kiểm

| Dữ kiện | Nguồn |
|---|---|
| `TH_Data`: `Temperature` 0\|16 `@1-` ×0,1 · `Humidity` 16\|16 `@1+` ×0,1 · `DataAge` 32\|16 `@1+` ×0,1 s · `Counter` 48\|8 · `Flags` 56\|8. DLC 8, Intel | DBC `BO_ 1024` |
| `TH_Status`: `FwVersion` 0\|8 · **`StatusFlags`** 8\|8 · `NodeId` 16\|8 · **`TxPeriod`** 24\|8 **×10 ms** · `ModbusOk` 32\|16 · `ModbusErr` 48\|8 · `CmdSeq` 56\|8 | DBC `BO_ 1026` |
| **Tên trong DBC khác proposal:** proposal §5.2 gọi là `Flags` và `TxPeriod_x10ms`; DBC gọi là **`StatusFlags`** và **`TxPeriod`** (hệ số 10, đơn vị ms). Cùng byte, cùng giá trị thô (100 = 1000 ms). **DBC là thứ người phân tích dùng ⇒ code và tài liệu theo tên DBC** | DBC, proposal |
| `DataAge` = 6553,5 nghĩa là "chưa đọc được lần nào" — tức là **`TH_Data` được phát cả trước lần đọc đầu tiên** | DBC `CM_ SG_ 1024 DataAge` |
| `Counter`: "một bước nhảy nghĩa là frame bị mất trên đường truyền, KHÔNG phải cảm biến bị mất — để phân biệt, xem `DataAge`" | DBC `CM_ SG_ 1024 Counter` |
| `StatusFlags`: "cùng bit như `Flags` của `TH_Data`" | DBC `CM_ SG_ 1026 StatusFlags` |
| `NodeId` lặp lại `node_id` **có chủ ý**: cách duy nhất phát hiện hai module trùng ID | DBC `CM_ SG_ 1026 NodeId` |
| `MX_CAN_Init` đặt `CAN_MODE_NORMAL`; gọi lại `HAL_CAN_Init` với `CAN_MODE_SILENT_LOOPBACK` **trước** `HAL_CAN_Start` thì không chạy lại MspInit (state đã READY), không sửa file sinh ra | `can.c`; module góc GĐ 5 đã chạy thật |
| NVIC đã bật `CAN_RX0` và `CAN_SCE` từ `.ioc`, nhưng **không ngắt CAN nào được kích hoạt** (không gọi `HAL_CAN_ActivateNotification`, IER = 0). FIFO đọc bằng thăm dò, như module góc | `can.c`, `.ioc` |

---

## 2. Thiết kế

### 2.1 Đường dữ liệu — đồng hồ 1 Hz, không theo sự kiện

```
app_th (2 s) --> app_meas: so do cuoi + thoi diem
                                |
app_can_task, moi 1000 ms ------+--> cong: APP_NODE_ID_OK(node)?
                                |      khong -> gate_blocked++, khong gi ca
                                +--> chup: T, RH, DataAge, Flags, ModbusOk, ModbusErr  (CUNG MOT tick)
                                +--> app_frame_pack_data / _status  (thuan, 8 byte)
                                +--> HAL_CAN_AddTxMessage x 2: 0x400 roi 0x402
                                '--> drain FIFO0: so tung byte voi ban da gui
```

**Khác module góc, có chủ ý.** Bên đó frame dữ liệu đi theo sự kiện (có ảnh mới mới phát) để chặn dữ liệu đóng băng. Ở đây hợp đồng chọn cách ngược lại: `TH_Data` đi **đều mỗi giây dù cảm biến sống hay chết**, và **`DataAge` mang sự thật** (proposal §5.1, DBC `CM_`). Một cảm biến chết vẫn ra frame — nhưng frame đó tự nói nó cũ bao nhiêu. Im lặng thay vì phát sẽ làm mất chính thông tin "cảm biến đã chết lúc nào".

**Chụp một lần, đóng hai frame.** T, RH, `DataAge`, `Flags` lấy ở **cùng một tick** rồi đóng vào cả `TH_Data` lẫn `TH_Status` (`StatusFlags` = `Flags`). Hai frame cùng giây không bao giờ nói hai điều khác nhau.

### 2.2 Byte — từ DBC v1.5, node `0x30`

| Frame | Byte | Signal | Nguồn |
|---|---|---|---|
| `TH_Data` `0x400` | 0–1 | `Temperature` `@1-` | `app_meas.t_x10` — **thẳng từ thanh ghi**, không đổi đơn vị |
| | 2–3 | `Humidity` `@1+` | `app_meas.rh_x10` |
| | 4–5 | `DataAge` `@1+` | `app_meas_age_x100ms(now)` — `0xFFFF` = chưa đọc |
| | 6 | `Counter` | mục 2.4 |
| | 7 | `Flags` | `app_meas_flags(now)` \| `BUS_OFF_SEEN` |
| `TH_Status` `0x402` | 0 | `FwVersion` | `APP_FW_VERSION` (1) |
| | 1 | `StatusFlags` | **đúng bằng** byte 7 của `TH_Data` cùng giây |
| | 2 | `NodeId` | `node_id` đang dùng |
| | 3 | `TxPeriod` (×10 ms) | `APP_TX_PERIOD_X10MS` = 100 |
| | 4–5 | `ModbusOk` | `app_meas_modbus_ok()` — 16 bit, vòng |
| | 6 | `ModbusErr` | `app_meas_modbus_err()` — **bão hoà** 255 |
| | 7 | `CmdSeq` | 0 — lệnh là giai đoạn 6 |

`base + 1` và `base + 3` (`0x401`, `0x403`) **để trống** theo ID Map §2.1.

### 2.3 `app_can` — loopback, lập lịch, so sánh

- **Mode.** `app_can_init()`: `hcan.Init.Mode = CAN_MODE_SILENT_LOOPBACK`, `HAL_CAN_Init()` lần nữa, filter, `HAL_CAN_Start()`. **Silent loopback là hằng số của giai đoạn 3, không có công tắc.**
- **Filter**: một bank, nhận mọi ID vào FIFO0 — chỉ để loopback có chỗ rơi. Khớp ID bằng phần mềm.
- **Mọi `HAL_CAN_*` kiểm `HAL_StatusTypeDef`.** Init, filter hay start hỏng ⇒ `app_fault_raise(APP_FAULT_CAN, bước << 8 | mã lỗi HAL)`, **không fatal**: đo vẫn chạy, chỉ không nói được. **`APP_FAULT_CAN = 5`** — trùng số với module góc (nguyên tắc ở `app_fault.h`: nghĩa trùng thì số trùng); 3 vẫn để dành cho config, 4 không dùng.
- **Mailbox**: mỗi frame cần một hộp trống; không có ⇒ **bỏ frame đó**, `tx_full++`. Không phát lại frame cũ — frame trễ đứng sau frame mới trên dây tệ hơn mất.
- **Cổng đứng trước mọi thứ chạm bxCAN**: `node_id` không qua `APP_NODE_ID_OK()` ⇒ không dựng, không `AddTxMessage`, `gate_blocked++` mỗi giây.
- **Drain trước, gửi sau**: giữ bản sao 8 byte của frame gửi gần nhất cho mỗi ID trong hai ID. Byte về khác ⇒ `rx_mismatch`; ID lạ ⇒ `rx_unexpected`.
- **`ESR` mỗi giây** (TEC, REC, LEC, BOFF / EPVF / EWGF) vào `g_diag.can`. Loopback phải cho toàn 0. Đây là mốc để giai đoạn 4 so khi lên dây thật.
- **`BUS_OFF_SEEN`** (`Flags` bit 5): chốt nếu `ESR.BOFF` từng bật. Ở loopback không thể xảy ra; viết ngay để giai đoạn 4 không phải chạm lại đường đóng gói.

### 2.4 `Counter` — tăng theo **khe 1 s**, không theo frame vào mailbox

`Counter` tăng 1 mỗi lần tới hạn 1 s **khi cổng mở**, dù frame có vào được mailbox hay không. Hệ quả cho người đọc `.blf`:

| Thấy | Nghĩa |
|---|---|
| `Counter` liền, cách nhau 1 s | bình thường |
| `Counter` **nhảy** | một frame đã tới hạn mà **không tới log**: mailbox đầy phía module (`tx_full`) hoặc mất trên bus |
| `DataAge` tăng dần, `Counter` vẫn liền | frame tới đều, **cảm biến** không trả lời |

Đúng nghĩa DBC ghi ("bước nhảy = frame bị mất trên đường truyền"). Nếu chỉ tăng khi đã vào mailbox thì một frame bị bỏ vì mailbox đầy sẽ **không để lại dấu vết nào** trong log — chính loại hỏng vô hình CLAUDE.md §6.3 cấm.

### 2.5 `node_id` ở giai đoạn 3

Chưa có kho config (giai đoạn 4), nên `node_id` là hằng **`APP_CAN_NODE_ID_PHASE3 = 0x30`** (nhãn 1) trong `app_config.h`.

- Vô hại **chỉ vì** giai đoạn 3 không có đường ra dây (silent loopback + transceiver standby).
- `g_diag.can.node_id_source = 1` (PHASE3) ở mọi lần đọc.
- **Giai đoạn 4 xoá hằng này** và thay bằng config đã kiểm CRC. Không còn nguồn `node_id` nào khác.
- Bài test cổng đổi hằng này thành `0xFF`.

---

## 3. File

| File | | Việc |
|---|---|---|
| `Core/{Inc,Src}/app/app_frame.*` | **mới, thuần** | ID từ `node_id`, cổng, `pack`/`unpack` hai frame |
| `Core/{Inc,Src}/app/app_can.*` | **mới** | silent loopback, filter, lịch 1 Hz, so byte, ESR, `BUS_OFF_SEEN` |
| `app_diag.h` | sửa | `g_diag.can` |
| `app_fault.h` | sửa | `APP_FAULT_CAN = 5` |
| `app_config.h` | sửa | `APP_FW_PHASE "3"`, `APP_CAN_NODE_ID_PHASE3`, `APP_FRAME_DLC`, offset ID, ID của CAN trong giai đoạn 3 |
| `main.c` | sửa, **USER CODE** | `app_can_init()` sau `app_th_init()`; `app_can_task()` sau `app_th_task()` |
| `tests/test_app_frame.c`, `tests/test_dbc_layout.c`, `Makefile` | mới / sửa | mục 6 |
| `docs/VF_TH_Phase3_Test.md`, `…_Report.md` | mới | sau khi code |

**Hai file `.c` mới ⇒ Refresh + Build Project trong IDE một lần**, như giai đoạn 2.

## 4. API

```c
/* app_frame.h - pure */
typedef struct {
  int16_t  t_x10;  uint16_t rh_x10;  uint16_t age_x100ms;  uint8_t counter;  uint8_t flags;
} app_frame_data_t;
typedef struct {
  uint8_t fw_version, status_flags, node_id, tx_period_x10ms;
  uint16_t modbus_ok;  uint8_t modbus_err, cmd_seq;
} app_frame_status_t;

bool     app_frame_node_ok(uint8_t node_id);             /* = APP_NODE_ID_OK, the only rule */
uint32_t app_frame_id_data(uint8_t node_id);             /* base + 0 */
uint32_t app_frame_id_status(uint8_t node_id);           /* base + 2 */
void     app_frame_pack_data  (const app_frame_data_t *d,   uint8_t out[APP_FRAME_DLC]);
void     app_frame_pack_status(const app_frame_status_t *s, uint8_t out[APP_FRAME_DLC]);
void     app_frame_unpack_data  (const uint8_t in[APP_FRAME_DLC], app_frame_data_t *d);    /* tests */
void     app_frame_unpack_status(const uint8_t in[APP_FRAME_DLC], app_frame_status_t *s);

/* app_can.h */
void app_can_init(void);
void app_can_task(uint32_t now_ms);
```

## 5. Ngân sách

| | Ước tính |
|---|---|
| Tải bus (khi lên dây ở GĐ 4) | 2 frame / s ≈ **0,04%** — 0,02 frame-slot (CLAUDE.md §5 tính 0,01 cho `TH_Data`; `TH_Status` cũng 1 Hz) |
| Flash | **+3–5 KB** ở `-O0` — lần này tính cả HAL CAN (`AddTxMessage`, `GetRxMessage`, `ConfigFilter`, `Start`), bài học ước thấp ở GĐ 2 |
| RAM | ~100 B (`g_diag.can`, hai bản sao 8 byte) |

## 6. Test trên PC

| Test | Chứng minh |
|---|---|
| ID: `0x30` → `0x400` / `0x402`; `0x39` → `0x490` / `0x492` | công thức ID Map |
| Cổng: `0x2F`, `0x3A`, `0x29`, `0x40`, `0xFF` bị từ chối; `0x30`…`0x39` được nhận | `APP_NODE_ID_OK` là nguồn duy nhất |
| **Byte viết tay từ DBC** (dưới) | layout, dấu, thứ tự byte |
| Đẩy `0xFF` qua **từng trường một** | các trường không đè nhau — khứ hồi pack → unpack không bắt được lỗi đó |
| Khứ hồi pack → unpack | đối xứng |
| **`test_dbc_layout`: đọc `contracts/VF_E-Drive_Data_Logger_v1.5.dbc`**, tìm `BO_ 1024` / `BO_ 1026`, so **bit đầu, độ dài, dấu, thứ tự byte, hệ số** của từng `SG_` với bảng layout của `app_frame` | **Mới so với module góc.** `check_contracts` bắt được khi file DBC đổi; test này bắt được khi **firmware không theo kịp** sau khi ai đó `--update` hash. Hai chốt chặn, hai việc khác nhau |

```
TH_Data   T=-205 RH=258 age=15 cnt=0x2A flags=0x10             -> 33 FF 02 01 0F 00 2A 10
TH_Data   chua doc: T=0 RH=0 age=0xFFFF cnt=0x00 flags=0x03     -> 00 00 00 00 FF FF 00 03
TH_Status fw=1 sf=0x0A node=0x30 tp=100 ok=0x1234 err=255 seq=0 -> 01 0A 30 64 34 12 FF 00
```

## 7. Test trên board (phác thảo — bản đầy đủ sau khi code)

Không cần bus CAN hay analyzer cho phần chính — mọi thứ quay về trong chip. Cần cảm biến và 12 V như giai đoạn 2.

| # | Làm gì | Mong đợi |
|---|---|---|
| 1 | Chạy 60 s (không Suspend), đọc `g_diag.can` | `mode` = silent loopback; `node_id` = `0x30`; `data_sent` ≈ `status_sent` ≈ 60; `rx_frames` = tổng; **`rx_mismatch` = 0, `rx_unexpected` = 0**; `tx_full` = 0; TEC = REC = 0; `gate_blocked` = 0 |
| 2 | **Giải mã tay** `last_data[8]` và `last_status[8]` | byte 0–1 = `t_x10` little-endian (25,4 °C ⇒ `FE 00`); 2–3 = `rh_x10` (51,0 %RH ⇒ `FE 01` — byte cao khác 0, thứ tự byte lộ ra); 4–5 = tuổi; byte 7 của data **bằng** byte 1 của status; status byte 2 = `30`, byte 3 = `64` |
| 3 | **Rút J1** 15 s trong lúc xem `last_data` | byte 4–5 (`DataAge`) tăng mỗi giây; byte 7 thêm bit 3 (`0x08`) sau 6,1 s; `Counter` (byte 6) **vẫn liền** — frame vẫn đi, cảm biến thì không |
| 4 | **Cổng**: `APP_CAN_NODE_ID_PHASE3 = 0xFF`, Clean, Build, 60 s | `data_sent` = `status_sent` = `rx_frames` = 0; `gate_blocked` ≈ 60. **`*_sent` khác 0 là lỗi nghiêm trọng nhất có thể có.** Trả lại `0x30` |
| 5 | Hồi quy: analyzer trên bus cô lập 60 s; SWD; đọc cảm biến như GĐ 2 | **không một frame nào trên dây**; mọi thứ của GĐ 2 vẫn đạt |

---

## 8. Rủi ro

| Rủi ro | Xử lý |
|---|---|
| Ai đó thêm hàm đưa transceiver sang Normal "để thử" | Không có hàm đó; báo cáo và CLAUDE.md §7.1 ghi rõ nó đến cùng config CRC ở GĐ 4 |
| Loopback **không thể thấy** dữ liệu đóng băng — byte về luôn khớp byte đi, kể cả khi cả hai cũ | Phòng tuyến là `DataAge`, không phải loopback. Bài 3 kiểm đúng điều đó trên byte thô |
| Byte order sai mà không lộ (bài học module rung: mọi giá trị < 256) | Ở đây RH ≈ 510 và T ≈ 254 đều có byte cao/thấp khác nhau; tuổi `0xFFFF` lúc chưa đọc; test host có giá trị âm |
| `HAL_CAN_Init` lần hai chạy lại MspInit | Không: state đã READY sau `MX_CAN_Init` (đã chạy thật trên module góc) |
| Tên signal trong code lệch DBC (`Flags` / `TxPeriod_x10ms` của proposal) | Code dùng tên DBC; `test_dbc_layout` so theo tên DBC |

## 9. Cố tình KHÔNG làm

| | Vì sao |
|---|---|
| Không NORMAL, không transceiver Normal | Giai đoạn 4, cùng kho config có CRC |
| Không nhận lệnh `TH_Cmd`, `CmdSeq` = 0 | Giai đoạn 6 |
| Không ngắt CAN | Thăm dò FIFO từ vòng lặp chính là đủ cho 2 frame / s |
| Không `FORCE_READ` | Giai đoạn 6 |

---

## 10. Cần bạn quyết

### Câu hỏi 1 — `TH_Data` trước lần đọc đầu tiên mang số gì?

Hợp đồng nói `DataAge` = `0xFFFF` nghĩa là chưa đọc — tức frame có được phát lúc đó. Nhưng **không nói `Temperature` và `Humidity` mang gì**.

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | **`0` / `0`**, kèm `DataAge` = 6553,5 và `Flags` = `NEVER_READ` (\| `TIMEOUT`) | Đúng chữ hợp đồng, không thêm quy ước nào. Ai vẽ đồ thị nhiệt độ mà không đọc `DataAge` sẽ thấy 0 °C trong ~2 s đầu — **đúng loại sai mà DBC `CM_` đã dặn "đọc DataAge trước"** |
| B | Giá trị canh: `Temperature` = `0x7FFF`, `Humidity` = `0xFFFF` (ngoài dải DBC) | Đồ thị vọt lên, không thể nhầm. **Nhưng là quy ước mới chưa có trong hợp đồng** ⇒ phải sửa DBC (`VAL_`) và proposal ở `VF_Main`, bump version, đồng bộ cả ba repo |

### Câu hỏi 2 — `Counter` tăng theo khe 1 s hay theo frame vào mailbox?

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | **Theo khe 1 s** (mục 2.4) | Mọi frame đã tới hạn mà không tới log đều để lại một bước nhảy — kể cả bị bỏ vì mailbox đầy. Giống cách module góc chọn |
| B | Chỉ khi đã vào mailbox | Giống module rung. Frame bị bỏ ở mailbox **không để lại dấu** trong log; chỉ `tx_full` trong `g_diag` biết |
