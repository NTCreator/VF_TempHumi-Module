# Giai đoạn 3 — Báo cáo

**Trạng thái: XONG 2026-10-07** — build sạch, test host 1 577/1 577, test board đạt cả 8 bài (mục 6).

`TH_Data` (`0x400`) và `TH_Status` (`0x402`) đi qua bxCAN thật mỗi giây và quay về nguyên vẹn từng byte qua **silent loopback**; transceiver vẫn standby. Không một bit nào lên dây — đã kiểm bằng analyzer. Phát thật là giai đoạn 4, cùng kho config có CRC.

## 1. Kế hoạch đã duyệt

`docs/VF_TH_Phase3_Plan.md`, duyệt 2026-10-07, cả hai câu hỏi **phương án A**:

| Câu hỏi | Quyết định |
|---|---|
| 1. T / RH trước lần đọc đầu | **`0` / `0`**, kèm `DataAge` = `0xFFFF` (6553,5 s) và `NEVER_READ` — đúng chữ hợp đồng, không thêm quy ước |
| 2. `Counter` | **Tăng theo khe 1 s** khi cổng mở, dù mailbox có nhận frame hay không: frame nào tới hạn mà không tới log đều để lại bước nhảy |

Không có chỗ làm khác kế hoạch.

## 2. File đã tạo / sửa

| File | Việc |
|---|---|
| `Core/{Inc,Src}/app/app_frame.*` | **Mới, thuần.** ID (`base + 0`, `base + 2`), cổng `node_id` (gọi `APP_NODE_ID_OK`, nguồn duy nhất), pack / unpack hai frame. Tên trường theo **DBC** (`StatusFlags`, `TxPeriod`), không theo proposal §5.2 |
| `Core/{Inc,Src}/app/app_can.*` | **Mới.** `hcan.Init.Mode = CAN_MODE_SILENT_LOOPBACK` + `HAL_CAN_Init` lần hai (MspInit không chạy lại — đã đọc mã HAL), filter nhận mọi ID vào FIFO0, `HAL_CAN_Start`. Mỗi giây: đọc ESR, cổng, **chụp một lần — đóng hai frame**, `AddTxMessage` ×2. Mỗi vòng: drain FIFO0 (giới hạn 4), so từng byte với bản sao. Mọi `HAL_CAN_*` kiểm status; mọi lối ra tăng đúng một bộ đếm |
| `app_config.h` | `APP_FW_PHASE "3"`, offset ID, `APP_FRAME_DLC`, **`APP_CAN_NODE_ID_PHASE3 = 0x30`**, `APP_CMD_SEQ_NONE`. `_Static_assert` khoá `0x30` → `1024` / `1026` (DBC `BO_`) |
| `app_fault.*` | **`APP_FAULT_CAN = 5`** — chốt, không fatal; trùng số và nghĩa với module góc. 3 và 4 để trống có ghi lý do. Sửa luôn: thiếu dấu phẩy sau mã 2 khi chèn mã 5 (build thử bắt được, chưa từng lên board) |
| `app_diag.h` | `g_diag.can` |
| `main.c` | Chỉ USER CODE: `app_can_init` sau `app_th_init`; `app_can_task` sau `app_th_task` |
| `tests/test_app_frame.c`, `tests/test_dbc_layout.c`, `Makefile` | Mục 4 |
| `docs/VF_TH_Phase3_Test.md` | Quy trình test |

Không đụng `.ioc`, `Drivers/`, `contracts/`, `.cproject`.

## 3. Thiết kế đã làm

- **`TH_Data` đi đều mỗi giây, cảm biến sống hay chết** — khác module góc (bên đó phát theo sự kiện). Hợp đồng chọn: frame vẫn đi, `DataAge` nói sự thật. Bài 3 và bài 4 trên board cho thấy đúng điều đó trong byte thô.
- **Một tick, hai frame**: T, RH, `DataAge`, `Flags`, `ModbusOk`, `ModbusErr` lấy cùng lúc ⇒ `StatusFlags` == `Flags` cùng giây. Bài 7 trên board: cả hai = `03`.
- **Mailbox không trống ⇒ bỏ frame, `tx_full++`**, không phát lại.
- **`BUS_OFF_SEEN`** chốt khi `ESR.BOFF` từng bật, OR vào `Flags`. Không thể xảy ra ở loopback; viết sẵn cho giai đoạn 4.
- **Hai lớp im lặng**, như giai đoạn 1: silent loopback (CANTX recessive, không ACK) + transceiver standby, và **vẫn không có hàm nào** đưa transceiver sang Normal.

## 4. Test trên host — 1 577 / 1 577

```
test_drv_modbus: 193 checks, 0 failures
test_app_meas:   169 checks, 0 failures
test_app_frame:  135 checks, 0 failures
test_dbc_layout: 1080 checks, 0 failures
```

| Test | Nội dung |
|---|---|
| `test_app_frame` | ID 10 node; cổng (`0x30..0x39` nhận; `0x2F`, `0x3A`, `0x3F`, `0x29`, `0x20`, `0x19`, `0x40`, `0x00`, `0xFF` từ chối); **byte viết tay từ DBC**: số datasheet tr. 7 (`33 FF 02 01 0F 00 2A 10`), frame chưa đọc (`00 00 00 00 FF FF 00 03`), số board GĐ 2 (`FE 00 FE 01 …`), `TH_Status` (`01 0A 30 64 34 12 FF 00`); `0xFF` qua **từng trường một**; khứ hồi kể cả `INT16_MIN` / `MAX` |
| `test_dbc_layout` | **Đọc thẳng `contracts/*.dbc`** (Makefile truyền đường dẫn; 0 hay 2 file DBC ⇒ từ chối chạy). Với **10 module × 2 frame**: ID, DLC 8, sender `TH_xx`; **tập tên signal** đúng bằng nhau; mỗi signal: Intel, dấu, hệ số, offset, đơn vị; **bit firmware ghi** cho trường (toàn 1) **đúng bằng bit DBC đặt tên**; **mẫu `0x12A5` rơi đúng bit đầu, byte thấp trước** (thứ tự byte); các signal lát kín 64 bit, không chồng |

`check_contracts` bắt được khi **file DBC** đổi. `test_dbc_layout` bắt được khi **firmware không theo kịp** sau khi ai đó chép DBC mới và `--update` hash — hai chốt, hai việc khác nhau.

### 4.1 Đột biến — 11 / 11 bị bắt

| Phía | Đột biến | Bắt bởi |
|---|---|---|
| firmware | đóng gói big-endian | `test_app_frame` 13 fail, `test_dbc_layout` 40 fail |
| firmware | đổi chỗ `Counter` / `Flags` | 7 / 20 fail |
| firmware | `TH_Status` ở `base + 1` | 3 / 10 fail |
| DBC | `Humidity` dời sang bit 24 | 30 fail |
| DBC | `DataAge` hệ số 0,01 | fail |
| DBC | `TH_Status_01.StatusFlags` đổi tên `Flags` | fail |
| DBC | `Temperature` thành không dấu | fail |
| DBC | `TH_Status_05` dời ID | fail |
| DBC | `TxPeriod` đơn vị `s` | fail |
| DBC | thêm signal vào `TH_Data_07` | fail |
| DBC | `Humidity` thành Motorola (`@0`) | fail |

**Một lỗ tìm ra nhờ đột biến:** bản đầu của `test_dbc_layout` chỉ dùng mẫu toàn 1 — trông giống nhau theo cả hai thứ tự byte, nên firmware big-endian **lọt qua** (0 fail). Đã thêm mẫu `0x12A5`; giờ 40 fail. (Hai đột biến DBC đầu tiên cũng đã sửa nhầm `ANG_Status` — module góc có signal cùng tên; làm lại nhắm đúng khối `TH_Status_01`.)

## 5. Build và bộ nhớ

Người dùng build trong IDE và chạy trên board (log không dán). **Rebuild sạch headless** sau khi trả `APP_CAN_NODE_ID_PHASE3` về `0x30`: **0 error, 0 warning**, `check_contracts: ok`. Code ứng dụng sạch thêm với `-Wextra -Wconversion -Wshadow -Wdouble-promotion` ở `-O0` và `-Os`. Không có hàm `double` trong image.

```
   text    data     bss     dec     hex  filename
  26488      12    2172   28672    7000  VF_TempHumi_Module.elf      (Debug, -O0)
```

| | GĐ 2 | GĐ 3 | Có | Tỷ lệ |
|---|---|---|---|---|
| Flash | 22 948 | **26 500** | 53 248 (vùng ứng dụng sau phase 5) | **49,8%** |
| RAM tĩnh | 512 | **648** | 12 288 | 5,3% |

**+3,5 KB flash**, trong ước lượng +3–5 KB của kế hoạch (lần này đã tính cả HAL CAN). `app_can` 1 388 B, `app_frame` 574 B; phần còn lại là HAL CAN (`AddTxMessage`, `GetRxMessage`, `ConfigFilter`, `Start`). `g_diag` lên 204 B.

Stack: đường sâu nhất mới `app_can_task` 24 → `can_send_both` 72 → `can_send` 56 → `HAL_CAN_AddTxMessage` — nông hơn đường quét của giai đoạn 2 (~320 B), không đổi kết luận báo cáo GĐ 2 §5.1.

## 6. Kết quả test trên board — 2026-10-07

| # | Bài | Kết quả |
|---|---|---|
| 0 | 12 V, chung âm | **PASS** |
| 1 | 60 s | **PASS** — `status` 0, `mode` 1, `node_id` 48 (`0x30`), `source` 1; **`slots` = `data_sent` = `status_sent` = 61**; **`rx_frames` = `rx_match` = 122**; `rx_mismatch` = `rx_unexpected` = `rx_errors` = `tx_full` = `tx_errors` = `gate_blocked` = `not_running` = 0; `counter` = 60 (= `slots` − 1, bắt đầu từ 0); TEC = REC = LEC = ESR = 0; ID 1024 / 1026 |
| 2 | Giải mã tay | **PASS** — `last_data` = `0D 01 E6 01 09 00 70 00`: T = `0x010D` = **269** = `g_diag.th.t_x10` (26,9 °C); RH = `0x01E6` = 486 (48,6 %RH; `g_diag.th` chụp lúc khác: 483); tuổi 0,9 s; `Counter` 112; `Flags` 0. **Byte cao `01` khác 0 ⇒ thứ tự byte được chứng minh trên phần cứng.** `last_status` = `01 00 30 64 38 00 00 00`: fw 1, **`StatusFlags` = `last_data[7]`**, node `0x30`, `TxPeriod` 100, **`ModbusOk` = 56** — khớp `Counter` 112 s ÷ 2 s mỗi lần đọc, `ModbusErr` 0, `CmdSeq` 0 |
| 3 | Rút J1 15 s | **PASS** |
| 4 | Khởi động không cảm biến | **PASS** — frame đầu: **`00 00 00 00 FF FF 00 01`** (đúng từng byte bảng kế hoạch: T = RH = 0, `DataAge` = `FFFF`, `NEVER_READ`); vài giây sau: `00 00 00 00 FF FF 06 03` (`Counter` 6, thêm `TIMEOUT`) |
| 5 | Cổng `node_id` = `0xFF` | **PASS** — `node_id` 255; **`slots` = `data_sent` = `status_sent` = `rx_frames` = 0**; `gate_blocked` = 56; không ID nào được ghi |
| 6 | Hồi quy: analyzer trên bus | **PASS** — không một frame nào trên dây |
| 7 | `node_id` = `0x39` | **PASS** — ID **1168 / 1170** (`0x490` / `0x492`), `last_status[2]` = 57 (`0x39`); `rx_frames` = `rx_match` = 140. Lần chạy này không có cảm biến: `last_data` = `00 00 00 00 FF FF 45 03`, `last_status[1]` = 3 = `last_data[7]`, `ModbusErr` = 33 (≈ 69 s ÷ 2 s) — `StatusFlags` == `Flags` lần nữa |

Sau bài 7, `app_config.h` còn để `0x39u` — đã trả về **`0x30u`** trước commit (quy trình yêu cầu). **Board đang chạy image `0x39`**: nạp lại khi tiện.

## 7. Vấn đề còn mở

1. **Frame chưa từng lên dây thật** — giai đoạn 4. Loopback chứng minh bit timing, mailbox, layout; không chứng minh transceiver, dây, ACK của node khác.
2. **`BUS_OFF_SEEN`, `tx_full`** chưa từng bật trên phần cứng (không thể ở loopback).
3. **`APP_CAN_NODE_ID_PHASE3` phải bị xoá ở giai đoạn 4** — thay bằng config có CRC. Đây là nguồn `node_id` thứ hai mà hợp đồng không cho phép tồn tại lúc lên dây.
4. Từ giai đoạn 2, vẫn mở: độ chính xác RH (không có ẩm kế), đo DE bằng oscilloscope, số `latency_ms`. Từ giai đoạn 1: chung âm 12 V / 5 V trong hệ data logger, proposal TH chưa có nhà ở `VF_Main`, hai lỗi chữ trong hợp đồng, watchdog.
5. **Tải bus khi lên dây:** 2 frame / s ≈ 0,04% (0,02 frame-slot nếu tính cả `TH_Status`). CLAUDE.md §5 chỉ tính `TH_Data` (0,01) — đúng theo định nghĩa frame-slot của ID Map §2.2 ("frame dữ liệu"), ghi lại để không ai ngạc nhiên.
