# VF_TempHumi_Module — Giai đoạn 2: quy trình test trên board

**Image:** commit `phase2` (xem `git log`), `Debug/VF_TempHumi_Module.elf`, nạp ở `0x08000000`. `s_fw_id` = `"VF_TempHumi_Module phase 2"`.

Giai đoạn 2 chứng minh bốn điều:

1. Firmware **đọc được** nhiệt độ và độ ẩm, mỗi 2 s, không lỗi.
2. **DE hạ đúng lúc**: sau bit stop của byte cuối, không sớm hơn (cắt byte), không muộn hơn đáng kể.
3. **Số đo cũ thì biết là cũ**: rút dây thì tuổi tăng, cờ bật, LED đổi; cắm lại thì hồi phục.
4. Giai đoạn 1 **vẫn đúng**: CAN1 im lặng, SWD sống.

**Dụng cụ (người dùng, 2026-10-07):** oscilloscope 2 kênh; **không có ẩm kế tham chiếu**. Nhiệt độ so **tương đối** với cảm biến nhiệt độ phòng của lab. Độ ẩm chỉ kiểm được bằng **độ hợp lý** và **phản ứng** (bài 3) — ghi rõ như vậy trong kết quả, đừng ghi "đạt độ chính xác".

---

## 1. Chuẩn bị

### 1.1 Nguồn — như giai đoạn 1, cộng cảm biến

| | |
|---|---|
| +5 V | JP3 chân 1/2, GND chân 7/8 |
| +12 V | JP2 chân 5/6, âm chân 7/8 (`GND12`) |
| **Chung âm** | **Nối âm 12 V với âm 5 V** — trên board `GND12` không nối `GND` (CLAUDE.md §4.2). Không nối thì RS485 có thể chạy hoặc không, tuỳ may rủi |
| Cảm biến | vào J1: 1 = +12, 2 = B (xanh dương), 3 = GND12 (đen), 4 = A (vàng/xanh lá) — màu dây theo datasheet tr. 2 |

**Nạp khi chưa cấp +12 V** (JP2 chân 5/6 nằm sát SWD). Nạp xong mới cấp 12 V.

### 1.2 Live Expressions — "console" của board

CubeIDE → Debug → cửa sổ **Live Expressions** → thêm `g_diag`. Mở `g_diag.th`.

| Trường | Nghĩa |
|---|---|
| `g_diag.magic` | phải là `0x54484431` ("THD1") — nếu không, đang nhìn sai chỗ |
| `th.t_x10`, `th.rh_x10` | số đo hợp lệ cuối, ×10 (253 = 25,3 °C) |
| `th.age_x100ms` | **DataAge**, ×0,1 s. `65535` = chưa đọc được lần nào |
| `th.flags` | bit 0 NEVER_READ · bit 1 TIMEOUT · bit 2 CRC_ERR · bit 3 STALE · bit 4 RANGE |
| `th.fresh` | 1 = LED đang ở mẫu "đang đo" |
| `th.started` | số giao dịch đã bắt đầu |
| `th.count[0..9]` | 0 **OK** · 1 TIMEOUT · 2 SHORT · 3 CRC · 4 ADDR · 5 FUNC · 6 LEN · 7 EXCEPTION · 8 UART · 9 TX |
| `th.last_rx[]`, `th.last_rx_len` | byte thô của lần trả lời gần nhất, tốt hay xấu |
| `th.latency_ms`, `th.latency_max_ms` | từ lúc hạ DE tới byte trả lời đầu tiên |
| `th.scan.*` | kết quả quét chẩn đoán (bài 7) |

**Đừng Suspend (pause) giữa chừng.** MCU dừng, cảm biến thì không: vài giao dịch sẽ thành lỗi. Nếu lỡ pause, ghi lại và trừ ra.

---

## 2. Các bài

| # | Làm gì | Mong đợi | Nếu khác |
|---|---|---|---|
| **1** | Cảm biến cắm sẵn, cấp +5 V và +12 V, chạy. Quan sát 1 phút | LED: 2 Hz trong ~2 s đầu, rồi **1 nháy mỗi 5 s**. `count[0]` tăng 1 mỗi 2 s; **mọi `count[1..9]` = 0**; `age_x100ms` dao động 0…20; `flags` = 0; `fresh` = 1. Ghi `latency_ms` | `count[1]` tăng ⇒ không có trả lời: dây A/B, 12 V, chung âm. `count[3]` ⇒ nhiễu/dây dài. `count[8]` ⇒ ghi `last_uart_error`. Gửi lại `last_rx[]` |
| **2** | **Nhiệt độ so với cảm biến phòng lab.** Đặt cảm biến SHT cạnh cảm biến phòng, chờ **15 phút** cho cân bằng (thời gian đáp ứng ≤ 25 s, nhưng vỏ cần thời gian). Ghi 3 cặp số cách nhau 1 phút | Chênh lệch ổn định, \|Δ\| trong khoảng ±0,5 °C (SHT, datasheet tr. 1) **cộng** sai số của cảm biến phòng | \|Δ\| > 2 °C ⇒ báo lại, kèm vị trí đặt hai cảm biến |
| **3** | **Độ ẩm — hợp lý và phản ứng.** (a) Ghi `rh_x10` 1 phút khi để yên. (b) **Thở nhẹ** vào cảm biến 2–3 giây | (a) Trong khoảng phòng lab thường gặp (~30…80 %RH), dao động ≤ ±1 %RH. (b) RH **tăng rõ** trong ≤ 8 s (datasheet tr. 1), rồi về lại trong 1–2 phút. T tăng nhẹ, chậm hơn | Không đổi khi thở ⇒ nghi số bị đóng băng: kiểm `age_x100ms` có về 0 mỗi 2 s không |
| 3b | *(tuỳ chọn)* Nắm cảm biến trong tay 1 phút | T tăng vài độ trong ~25 s | |
| **4** | **Oscilloscope — thời điểm DE.** Kênh 1 = **PB6** (U4 chân 2 hoặc 3). Kênh 2 = **PA9** (U4 chân 4 = DI). Cả hai so **GND** (JP3-7). Trigger sườn **lên** kênh 1, single. Quét ~2 ms/div | PB6 lên **trước** sườn xuống đầu tiên của PA9 (bit start). 8 byte ≈ 16,7 ms. Byte cuối là `0x0B`: bit 7 = 0, nên PA9 có **một sườn lên cuối cùng** (vào bit stop). **PB6 xuống khoảng 208 µs (1 bit) sau sườn lên đó** — cho phép tới ~250 µs | PB6 xuống **trước hoặc ngay tại** sườn lên cuối ⇒ cắt bit stop: **dừng, báo lại** kèm ảnh. PB6 xuống muộn > 1 ms ⇒ báo lại |
| 4b | Giữ kênh 1 = PB6. Chuyển kênh 2 sang **PB7** (U4 chân 1 = RO). Quét ~5 ms/div | Sau khi PB6 xuống: PB7 im (cao), rồi **9 byte** trả lời. Khoảng lặng trước byte đầu ≈ `latency_ms`. Không có byte nào trong lúc PB6 cao (RE̅ tắt bộ thu) | Có hoạt động trên PB7 khi PB6 cao ⇒ báo lại |
| **5** | **Rút J1 30 s rồi cắm lại.** Theo dõi `flags`, `age_x100ms`, LED | Ngay sau lần hỏi kế: `flags` có bit 1 (TIMEOUT). Khi `age_x100ms` > 60 (6,0 s): thêm bit 3 (**STALE**), LED đổi sang **mã 2** (2 nháy, nghỉ 2 s). `count[1]` tăng ~15. Cắm lại: trong ≤ 2 s `count[0]` tăng, `age_x100ms` về 0, `flags` = 0, LED về 1 nháy / 5 s. **`t_x10`, `rh_x10` giữ số cũ suốt lúc rút** — tuổi mới là thứ nói nó cũ | LED không đổi khi rút ⇒ báo lại. Không hồi phục ⇒ ghi `count[]`, `last_rx[]` |
| **6** | **Khởi động khi chưa có cảm biến**: rút J1 (hoặc tắt 12 V), reset bằng SW1. Chờ 20 s. Rồi cắm cảm biến | LED 2 Hz tới ~10 s, rồi **mã 2**. `flags` = 3 (NEVER_READ + TIMEOUT), `age_x100ms` = 65535. Sau ~12 s quét chạy: `scan.ran` = 1, `scan.done` = 1, `scan.found` = 0, `scan.status` = {1, 1, 1} (timeout ở cả 3 baud), `baud` về **4800**. Cắm cảm biến: đọc được trong ≤ 2 s, LED "đang đo" | `scan.hal_err` = 1 ⇒ báo lại. `baud` ≠ 4800 sau quét ⇒ **dừng**, báo lại |
| **7** | **Tắt +12 V 30 s rồi bật lại** (cảm biến khởi động sau MCU) | Như bài 5 | |
| **8** | **Chạy liền 10 phút** | `count[0]` ≈ 300, **mọi `count[1..9]` = 0**, `latency_max_ms` ghi lại | Có lỗi ⇒ ghi chỉ số `count[]` nào và `last_rx[]` |
| **9** | **Hồi quy giai đoạn 1**: bus CAN cô lập (analyzer + module + 120 Ω), nghe 60 s. Rồi rút–cắm nguồn, ST-Link Connect | Không frame nào trên CAN1. SWD kết nối được | Có frame ⇒ **dừng**, báo lại |
| 10 | *(tuỳ chọn)* Đảo A/B ở J1 30 s rồi trả lại | Không bao giờ ra số đo; lỗi tăng (ghi loại: `count[1]`, `[2]`, `[3]` hay `[8]`). Trả lại ⇒ hồi phục | Đọc được số khi đảo A/B ⇒ báo lại |
| 11 | *(tuỳ chọn, cần bộ USB–RS485 trên PC)* Dùng PC đặt cảm biến sang **9600** (datasheet tr. 11). Khởi động module | Đọc **không** được (cố ý — firmware không tự đổi theo). Sau ~12 s: `scan.found` = 1, `scan.baud` = 9600, `scan.addr` = 1. **Rồi đặt cảm biến về 4800** | |

**Bài 4 là lý do giai đoạn này cần oscilloscope.** Cắt mất bit stop của byte cuối là lỗi "im lặng nhất": cảm biến bỏ qua câu hỏi, firmware đếm timeout, và mọi thứ trông như lỗi dây.

---

## 3. Bảng ghi kết quả — điền rồi gửi lại

| # | Kết quả (PASS / FAIL + ghi chú) |
|---|---|
| 1 | `latency_ms` = ___ ms |
| 2 | SHT / phòng lab: ___ / ___ °C, ___ / ___ °C, ___ / ___ °C |
| 3 | (a) RH = ___ %RH, dao động ±___ · (b) thở: RH ___ → ___ %RH trong ___ s |
| 4 | PB6 xuống sau sườn lên cuối của PA9: ___ µs (gửi ảnh nếu được) |
| 4b | |
| 5 | `count[1]` tăng ___; LED đổi sang mã 2: có / không; hồi phục sau ___ s |
| 6 | `scan.status` = {___, ___, ___}, `baud` = ___ |
| 7 | |
| 8 | `count[0]` = ___, `count[1..9]` = ___, `latency_max_ms` = ___ |
| 9 | |
| 10 | |
| 11 | |

---

## 4. Những gì quy trình này CHƯA kiểm được

- **Độ chính xác độ ẩm** — không có ẩm kế tham chiếu. Bài 3 chỉ chứng minh số **sống** và **phản ứng đúng chiều**.
- **Nhiệt độ âm** trên phần cứng — chỉ có test host (khung tr. 7 của datasheet, −20,5 °C).
- **Cờ RANGE** trên phần cứng — cần nhiệt độ ngoài −40…+60 °C. Chỉ có test host.
- **Mã exception Modbus** — cảm biến không trả exception cho câu hỏi đúng. Chỉ có test host.
- **Lỗi `TX` (TC không bao giờ tới)** — không gây ra được từ bên ngoài. Đường đó được đọc mã và đếm, chưa từng chạy thật.
