# VF_TempHumi_Module — Giai đoạn 3: quy trình test trên board

**Image:** commit `phase3` (xem `git log`), `s_fw_id` = `"VF_TempHumi_Module phase 3"`, `g_diag.fw_phase` = 3.

Giai đoạn 3 chứng minh:

1. `TH_Data` (`0x400`) và `TH_Status` (`0x402`) đi qua bxCAN **mỗi giây** và **quay về nguyên vẹn từng byte** qua silent loopback.
2. **Không một bit nào lên dây CAN1** — vẫn hai lớp: silent loopback + transceiver standby.
3. Byte trong frame **đúng DBC**, và **`DataAge` trong frame tăng khi cảm biến im** — frame tự nói nó cũ.
4. Cổng `node_id`: ID không hợp lệ ⇒ không frame nào.

Phần chính **không cần analyzer** — mọi frame quay về trong chip. Analyzer chỉ cần cho bài 6 (hồi quy: im lặng trên dây).

---

## 1. Chuẩn bị

Như giai đoạn 2: +5 V, +12 V, **chung âm**, cảm biến ở J1. Nạp khi chưa cấp 12 V.

Live Expressions: thêm `g_diag`, mở `g_diag.can` và `g_diag.th`. **Đặt hiển thị Hex** cho `last_data` và `last_status` (chuột phải → Number Format → Hex) — dễ đối chiếu với bảng dưới.

| `g_diag.can` | Nghĩa |
|---|---|
| `status` | 0 = chạy; 1/2/3 = init / filter / start hỏng (LED cũng sẽ nháy **mã 5**) |
| `mode` | 1 = silent loopback |
| `node_id`, `node_id_source` | 48 (`0x30`), 1 = hằng của giai đoạn 3 |
| `slots`, `data_sent`, `status_sent` | +1 mỗi giây |
| `rx_frames`, `rx_match` | +2 mỗi giây (hai frame quay về) |
| `rx_mismatch`, `rx_unexpected`, `rx_errors`, `tx_full`, `tx_errors`, `gate_blocked`, `not_running` | **phải là 0** |
| `tec`, `rec`, `bus_off_seen` | **phải là 0** |
| `last_data_id`, `last_status_id` | 1024 (`0x400`), 1026 (`0x402`) |
| `last_data[8]`, `last_status[8]` | byte của frame gửi gần nhất — bảng ở bài 2 |

---

## 2. Các bài

| # | Làm gì | Mong đợi | Nếu khác |
|---|---|---|---|
| **0** | Đo 12 V ở J1 (chân 1–3) và chung âm | ≈ 12 V, thông mạch | như GĐ 2 bài 0 |
| **1** | Chạy **60 s** (không Suspend) rồi đọc `g_diag.can` | `status` 0, `mode` 1, `node_id` 48; `slots` ≈ `data_sent` ≈ `status_sent` ≈ 60; `rx_frames` = `rx_match` ≈ 120; **mọi lỗi = 0**; `tec` = `rec` = 0. LED vẫn 1 nháy / 5 s. `g_diag.th.count[0]` vẫn tăng (GĐ 2 không bị ảnh hưởng) | `rx_mismatch` ≠ 0 ⇒ byte quay về khác byte đi: **dừng**, gửi `last_data` / `last_status`. LED mã 5 ⇒ ghi `status` và `app_fault` detail |
| **2** | **Giải mã tay** `last_data` và `last_status` (Hex), đối chiếu `g_diag.th` | Bảng dưới | Byte cao/thấp đảo ⇒ báo lại kèm ảnh |
| **3** | **Rút J1 15 s** trong lúc xem `last_data` | Byte 4–5 (`DataAge`, ×0,1 s) **tăng ~10 mỗi giây**; byte 7 (`Flags`) = `02` sau lần hỏi kế, rồi `0A` khi tuổi > 6,0 s; **byte 6 (`Counter`) vẫn +1 mỗi giây** — frame vẫn đi, cảm biến thì không. `rx_match` vẫn tăng. Cắm lại ⇒ tuổi về 0…20, `Flags` = `00` | Tuổi không tăng ⇒ **dừng** — đó đúng là lỗi "đóng băng" mà `DataAge` tồn tại để bắt |
| **4** | **Khởi động không cảm biến** (tắt 12 V, bấm SW1), đọc `last_data` sau ~1 s và sau ~5 s | ~1 s: `00 00 00 00 FF FF 00 01` (T = RH = 0, **`DataAge` = `FFFF`**, NEVER_READ). ~5 s: byte 7 = `03` (thêm TIMEOUT), byte 6 tăng dần. Bật 12 V ⇒ số đo thật xuất hiện, tuổi về 0 | Byte 4–5 ≠ `FF FF` khi chưa đọc lần nào ⇒ **dừng** |
| **5** | **Cổng `node_id`**: trong `app_config.h` đặt `APP_CAN_NODE_ID_PHASE3` = `0xFFu`, **Clean rồi Build**, nạp, chạy 60 s | `data_sent` = `status_sent` = `rx_frames` = `slots` = **0**; `gate_blocked` ≈ 60; `node_id` = 255. **`*_sent` khác 0 là lỗi nghiêm trọng nhất có thể có.** Đặt lại `0x30u`, Clean, Build, nạp lại | `*_sent` ≠ 0 ⇒ **dừng ngay** |
| **6** | **Hồi quy — im lặng trên dây**: bus cô lập (analyzer + module + 120 Ω), analyzer chế độ thường, nghe 60 s, rút–cắm nguồn module 3 lần. Rồi analyzer **phát** một frame `0x620` mỗi 100 ms trong 10 s | **Không một frame nào** từ module (không `0x400`, `0x402`, không error frame) — dù bên trong chip đang gửi 2 frame / s. Analyzer phát ⇒ **không có ACK** (GĐ 1 bài 8b) | Thấy `0x400`/`0x402` trên dây ⇒ **dừng ngay**, báo lại |
| 7 | *(tuỳ chọn)* `APP_CAN_NODE_ID_PHASE3` = `0x39u`, Clean, Build | `last_data_id` = 1168 (`0x490`), `last_status_id` = 1170 (`0x492`), `last_status[2]` = `39`. Đặt lại `0x30u` | |

### Bảng giải mã — bài 2

Ví dụ với số đo của GĐ 2 (25,4 °C, 51,0 %RH). Thay bằng số thật của bạn.

| `last_data` | Byte | Ví dụ | Đối chiếu |
|---|---|---|---|
| `Temperature` | 0–1 | `FE 00` | `0x00FE` = 254 = `g_diag.th.t_x10` |
| `Humidity` | 2–3 | `FE 01` | `0x01FE` = 510 = `g_diag.th.rh_x10` — **byte cao khác 0, nên thứ tự byte lộ ra ở đây** |
| `DataAge` | 4–5 | `0C 00` | 12 = 1,2 s, trong khoảng 0…20 (cảm biến trả lời mỗi 2 s) |
| `Counter` | 6 | `2A` | = `g_diag.can.counter`; +1 mỗi giây |
| `Flags` | 7 | `00` | = `g_diag.th.flags` |

| `last_status` | Byte | Ví dụ | Đối chiếu |
|---|---|---|---|
| `FwVersion` | 0 | `01` | `g_diag.fw_version` |
| `StatusFlags` | 1 | `00` | **= `last_data[7]`** — chụp cùng một tick |
| `NodeId` | 2 | `30` | `g_diag.can.node_id` |
| `TxPeriod` | 3 | `64` | 100 × 10 ms = 1000 ms |
| `ModbusOk` | 4–5 | `2C 01` | `0x012C` = 300 = `g_diag.th.modbus_ok` (lúc gửi) |
| `ModbusErr` | 6 | `00` | `g_diag.th.modbus_err` |
| `CmdSeq` | 7 | `00` | lệnh là giai đoạn 6 |

`g_diag.th` cập nhật liên tục, `last_*` chỉ cập nhật mỗi giây: tuổi và `ModbusOk` có thể lệch một nhịp — bình thường.

---

## 3. Bảng ghi kết quả

| # | Kết quả (PASS / FAIL + ghi chú) |
|---|---|
| 0 | |
| 1 | `slots` ___, `rx_match` ___, lỗi ___ |
| 2 | `last_data` = __ __ __ __ __ __ __ __ · `last_status` = __ __ __ __ __ __ __ __ |
| 3 | tuổi tăng ___ / s; `Flags` → `02` → `0A`: có / không; `Counter` liền: có / không |
| 4 | `last_data` lúc ~1 s = __ __ __ __ __ __ __ __ |
| 5 | `gate_blocked` ___, `*_sent` ___ |
| 6 | |
| 7 | |

## 4. Những gì quy trình này CHƯA kiểm được

- **Frame trên dây thật** — giai đoạn 4. Loopback chứng minh bit timing, mailbox, layout; nó **không** chứng minh transceiver, dây, ACK của node khác.
- **`BUS_OFF_SEEN`** — bus-off không thể xảy ra ở silent loopback. Đường chốt được đọc mã, chưa chạy thật.
- **`tx_full`** — 2 frame / s không bao giờ làm đầy 3 mailbox ở loopback.
- **Loopback không thấy dữ liệu đóng băng** — byte về luôn khớp byte đi, kể cả khi cả hai cũ. Phòng tuyến là `DataAge`, và bài 3 kiểm đúng điều đó.
