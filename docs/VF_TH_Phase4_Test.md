# VF_TempHumi_Module — Giai đoạn 4: quy trình test trên board

**Image:** commit `phase4` (xem `git log`), `s_fw_id` = `"VF_TempHumi_Module phase 4"` (+ tag nếu có công tắc bench), `g_diag.fw_phase` = 4.

Giai đoạn 4 chứng minh:

1. **Config không hợp lệ — kể cả page trắng — ⇒ im lặng trên CAN1**, trên một page trắng thật, với analyzer nghe.
2. **Config hợp lệ ⇒ lần phát thật đầu tiên**: `TH_Data_01` / `TH_Status_01` mỗi 1 s trên dây, giải mã bằng DBC v1.5.
3. **Không bao giờ có frame cũ lên dây**, kể cả khi mất ACK rồi có lại.
4. **Mainboard ghi được `.blf`** và giải mã được bằng DBC v1.5.

## ⚠️ Thứ tự là bắt buộc

**Bài 1 phải là lần nạp ĐẦU TIÊN của image giai đoạn 4, với build mặc định (mọi công tắc bench = 0).** Page config chỉ trắng **một lần**: bài 2 ghi vào nó. Module rung đã mất đúng phép thử này vì công tắc ghi mặc định là 1.

Lỡ ghi rồi? Build `APP_CFG_BENCH_ERASE = 1` một lần (bài 12), rồi làm lại từ bài 1.

---

## 1. Chuẩn bị

- **Nguồn** như GĐ 2–3: +5 V, +12 V, **chung âm**, cảm biến ở J1. Nạp khi chưa cấp 12 V.
- **Analyzer** (CANoe / TSMaster…) ở chế độ **thường — có ACK**, **không phải listen-only**, nạp **DBC v1.5** (`contracts/VF_E-Drive_Data_Logger_v1.5.dbc`), 500 kbit/s.
- **Bus cô lập cho bài 1–8**: analyzer + module + 120 Ω ở mỗi đầu (hoặc một 120 Ω nếu dây rất ngắn). **Không mainboard, không module nào khác.**
- **Start measurement ở analyzer TRƯỚC khi cấp nguồn module** — để thấy cả lúc khởi động.
- Đổi công tắc bench trong `Core/Inc/app/app_config.h` ⇒ **Clean rồi Build** (make so thời gian theo giây — CLAUDE.md của module góc §7.6).
- Live Expressions: `g_diag.cfg`, `g_diag.can`, `g_diag.th`.

| `g_diag.cfg` | Nghĩa |
|---|---|
| `state`, `state_at_boot` | 0 OK · **1 trắng** · 2 magic sai · 3 version · 4 CRC · 5 loại · 6 node · 7 reserved |
| `magic` | `826688340` = `0x31464354` "TCF1" · `4294967295` = `0xFFFFFFFF` trắng |
| `node_id`, `node_id_source` | 48 = `0x30`; nguồn 0 không có · 2 CONFIG · 3 BENCH |
| `crc_stored`, `crc_calc` | config bench: cả hai = `781977191` = `0x2E9C0667` |
| `writes`, `write_status`, `erase_status` | số lần ghi lần khởi động này; 0 OK · 1 hỏng · 255 không thử |

| `g_diag.can` mới | Nghĩa |
|---|---|
| `mode` | 1 silent loopback · **2 NORMAL** |
| `transceiver_on`, `stb_level` | 1 / 0 = Normal (PA10 thấp) · 0 / 1 = standby |
| `tx_aborted` | frame cũ cùng loại còn chờ trong mailbox đã bị huỷ |
| `rx_cmd_seen` | frame trên ID lệnh `0x040` (lệnh là GĐ 6 — chỉ đếm) |

---

## 2. Các bài

| # | Làm gì | Mong đợi | Nếu khác |
|---|---|---|---|
| 0 | 12 V ở J1, chung âm | như GĐ 2 bài 0 | |
| **1** | **ĐẦU TIÊN — page trắng.** Build mặc định, nạp. Analyzer chạy trước; cấp nguồn module, rút–cắm nguồn **3 lần** | `cfg.state` = **1**, `magic` = 4294967295, `node_id` 255, `source` 0, `writes` 0. `can.mode` 1, `transceiver_on` 0, `stb_level` 1, `slots` 0, `gate_blocked` +1 / s. **PA10 (R2 chân 2) ≈ 3,3 V.** **LED mã 3** (3 nháy, nghỉ 2 s). `g_diag.th.count[0]` vẫn tăng — cảm biến vẫn được đọc. **Analyzer: không một frame, không một error frame**, kể cả lúc cấp/ngắt nguồn | `state` ≠ 1 ⇒ page không trắng: bài 12, rồi làm lại. **Có frame ⇒ dừng ngay** |
| **2** | `APP_CFG_BENCH_WRITE = 1`, Clean, Build, nạp. Analyzer đang chạy | `state_at_boot` 1 → `state` **0**, `writes` 1, `write_status` 0, `source` **3**, `node_id` 48, `magic` 826688340, `crc_stored` = `crc_calc` = 781977191. `can.mode` **2**, `transceiver_on` **1**, `stb_level` 0, **PA10 ≈ 0 V**. LED: 1 nháy / 5 s. `s_fw_id` có ` BENCH_WRITE`. **Analyzer + DBC v1.5:** `TH_Data_01` (`0x400`) và `TH_Status_01` (`0x402`) **mỗi 1 s**: `Temperature` ≈ nhiệt độ phòng, `Humidity` hợp lý, `DataAge` 0…2 s, `Counter` +1 / s, `Flags` 0; `FwVersion` 1, `NodeId` 48, `TxPeriod` 1000 ms, `ModbusOk` tăng ~0,5 / s, `ModbusErr` 0. `can.tec` = 0 | `write_status` 1 ⇒ ghi flash hỏng: gửi cả `g_diag.cfg` |
| 3 | Bấm SW1 (không nạp lại) | `state_at_boot` **0**, **`writes` 0** — config sống qua reset, không bị ghi lại | `writes` 1 ⇒ config không sống qua reset: dừng |
| 4 | **Rút J1 15 s**, xem đồ thị `DataAge` và `Flags` ở analyzer, rồi cắm lại | `DataAge` leo đều ~1 s mỗi giây; `Flags` = 2 rồi **10** (TIMEOUT + STALE) sau 6,1 s; `Counter` vẫn liền. Cắm lại ⇒ `DataAge` về ≤ 2 s, `Flags` 0. **Lần đầu tiên thấy tuổi số đo trên dây thật** | |
| **5** | **Mất ACK.** Analyzer là node duy nhất khác: **Stop** analyzer (hoặc rút nó khỏi bus) **10 s**, rồi Start lại | Lúc mất: `can.tec` leo tới ~128 (error passive), **`tx_aborted` tăng ~2 mỗi giây** (mỗi loại một). Có lại: frame đầu tiên ở analyzer có `Counter` **nhảy** ~10 và **`DataAge` mới** (≤ 2 s) — **không có frame cũ nào đi trước**. `bus_off_seen` 0 | Thấy một loạt frame `Counter` liên tiếp đi ra ngay khi có lại, hoặc `DataAge` lớn ⇒ frame cũ lọt ra: báo lại |
| 6 | `WRITE = 0` lại (đường của sản phẩm), Clean, Build, nạp | `source` **2** (CONFIG), `writes` 0, phát như bài 2. `s_fw_id` không còn tag | |
| 7 | **Filter.** Từ analyzer gửi **một** frame `0x040` DLC 8, rồi **một** frame `0x041` | `rx_cmd_seen` +1 cho `0x040`; **`rx_frames` không đổi** cho `0x041` (filter phần cứng chặn). **Không bao giờ gửi `0x400` / `0x402` từ analyzer** — đó là ID của chính module, hai bên sẽ đụng nhau | |
| 8 | Tải bus ở analyzer | ≈ **0,04%** (2 frame / s) | |
| **9** | **Mainboard.** Bus = `VF_Main` (`41ad090`) + module này (analyzer listen-only nếu muốn xem). **Không module TH nào khác.** Module vẫn cần **+12 V riêng và chung âm với mainboard**. Ghi `.blf` **2 phút**; giữa chừng rút J1 30 s rồi cắm lại | Màn hình mainboard: module **`TEMPHUMI`**, node `0x30`, trạng thái **`OK`**; lúc rút J1: `TIMEOUT` rồi **`STALE`**; cắm lại: `OK`. Mở `.blf` bằng DBC v1.5: `TH_Data_01` 1 Hz, `Temperature` / `Humidity` đúng, `DataAge` leo trong 30 s rút dây, `Counter` liền; `TH_Status_01` có | Mainboard không thấy module ⇒ kiểm `can.mode` 2, `transceiver_on` 1, dây CAN_H/CAN_L |
| 10 | Hồi quy GĐ 2–3 | đọc cảm biến, LED, `g_diag.th` như trước | |
| 11 | *(tuỳ chọn)* `APP_BENCH_CAN_LOOPBACK = 1`, config hợp lệ | `mode` 1, `transceiver_on` 0, PA10 ≈ 3,3 V, analyzer **không thấy gì**, `rx_match` tăng như GĐ 3. Trả về 0 | |
| 12 | *(khi cần)* `APP_CFG_BENCH_ERASE = 1`: quay về page trắng | `erase_status` 0, `state` 1, im lặng như bài 1. **Trả về 0 ngay** — để 1 thì mỗi lần khởi động lại xoá page | |

**Xong thì mọi công tắc về 0.** Board giữ config bench (`node_id` `0x30`) trong flash và chạy đường của sản phẩm (bài 6).

---

## 3. Bảng ghi kết quả

| # | Kết quả (PASS / FAIL + ghi chú) |
|---|---|
| 1 | `state` ___, PA10 ___ V, LED mã 3: có / không, analyzer: ___ frame |
| 2 | `state` ___, `source` ___, PA10 ___ V, analyzer: `TH_Data_01` ___ Hz, T ___ °C, RH ___ % |
| 3 | `writes` ___ |
| 4 | |
| 5 | `tec` max ___, `tx_aborted` ___, `Counter` nhảy ___, frame cũ: có / không |
| 6 | `source` ___ |
| 7 | `rx_cmd_seen` ___ |
| 8 | tải ___ % |
| 9 | màn hình: ___ · `.blf`: ___ |
| 10 | |

## 4. Những gì quy trình này CHƯA kiểm được

- **Mất điện giữa lúc ghi flash** — không gây ra được một cách có kiểm soát. Thiết kế (page xoá xong thì trượt CRC) và test host (128 bit lật ⇒ không lần nào hợp lệ) là phòng tuyến.
- **Bus-off thật** — cần chập CAN_H/CAN_L khi đang phát; không bắt buộc. `AutoBusOff` đã bật, `BUS_OFF_SEEN` chốt.
- **Lệnh `TH_Cmd`** — GĐ 6. Bài 7 chỉ chứng minh filter đúng ID.
- **Cấu hình qua CAN2 từ mainboard** — GĐ 5 (bootloader).
