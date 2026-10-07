# VF_TempHumi_Module — Giai đoạn 1: quy trình test trên board

**Image:** commit `phase1` (xem `git log`), `Debug/VF_TempHumi_Module.elf`, nạp ở `0x08000000`.

Giai đoạn 1 không có chức năng nào để test. Nó tồn tại để chứng minh mấy điều mà mọi giai đoạn sau đều dựa vào:

1. **Board sống**, nạp được, và **SWD còn sống sau khi mất nguồn** (bẫy `VF_TH_IOC_Guide.md` §A3).
2. **PB6 = LOW** ⇒ MAX485 nhả bus RS485 (bẫy CLAUDE.md §4.3).
3. **PA10 = HIGH** ⇒ TJA1044 ở Standby.
4. **Module im lặng trên CAN1**: không phát, **không ACK**.
5. Cắm/rút cảm biến **không làm gì cả** — giai đoạn 1 chưa nói chuyện với cảm biến, nên "không có gì xảy ra" chính là kết quả đúng.

Mỗi bài có cột "Nếu khác". Board thật khác netlist thì **ghi lại sự thật đo được**, đừng sửa cho khớp tài liệu.

---

## 1. Chuẩn bị

### 1.1 Dụng cụ

- Đồng hồ vạn năng. *(Tuỳ chọn: oscilloscope cho bài 5b.)*
- ST-Link + CubeIDE hoặc CubeProgrammer.
- USB-CAN analyzer (CANoe / TSMaster…) và một điện trở 120 Ω.
- Nguồn bàn: **+5 V** và **+12 V**, có giới hạn dòng.

### 1.2 Nguồn — module cần HAI nguồn, và hai đất KHÁC NHAU

| Nguồn | Vào đâu | Dùng cho |
|---|---|---|
| **+5 V** | **JP3 chân 1/2**, GND vào **JP3 chân 7/8** | MCU (qua D1 → LDO U3), MAX485, TJA1044. Giới hạn dòng ~100 mA |
| **+12 V** | **JP2 chân 5/6**, âm vào **JP2 chân 7/8** (`GND12`) | **chỉ** cảm biến, qua J1. Giai đoạn 1 chỉ cần cho bài 9 |

> ⚠️ **`GND12` KHÔNG nối với `GND` ở bất kỳ đâu trên board** (netlist: net `GND12` chỉ có J1-3, JP2-7, JP2-8; sơ đồ nguyên lý dùng hai ký hiệu đất riêng). Tín hiệu RS485 A/B của cảm biến tham chiếu `GND12`, còn MAX485 tham chiếu `GND`.
>
> **Ở bàn test: nối âm của nguồn 12 V với âm của nguồn 5 V** (hoặc dùng một nguồn hai đầu ra chung âm). Hai nguồn cách ly thả nổi nhau thì giai đoạn 2 có thể đọc được hoặc không, tuỳ may rủi. Ghi lại: **trong hệ data logger, nguồn 12 V và nguồn 5 V có chung âm không?**

### 1.3 ST-Link vào JP2 — và cái bẫy 12 V ngay cạnh SWD

| JP2 | 1 | 2 | 3 | 4 | 5, 6 | 7, 8 |
|---|---|---|---|---|---|---|
| | GND | SWCLK | SWDIO | +3V3 | **+12** | GND12 |

- **Nạp khi CHƯA cấp +12 V.** Chân 5/6 là +12 V, nằm sát chân SWD. Một đầu cắm lệch hai chân là đưa 12 V vào ST-Link hoặc vào SWDIO.
- Chân 4 (+3V3) là **đầu ra** của LDO U3: chỉ nối vào chân **đo** điện áp (TVCC/VAPP) của ST-Link chính hãng; **không bao giờ** nối vào chân **cấp** 3,3 V của dongle clone.
- **JP2 không có NRST.** "Connect under reset" không dùng được. Nếu cần: chọn mode *Normal*, **giữ nút SW1**, bấm Connect, thả SW1.

### 1.4 Điểm đo

| Tín hiệu | Đặt que đo ở | So với |
|---|---|---|
| **PB6 / ENA_RS485** | **U4 (MAX485) chân 2 hoặc 3**, hoặc **R8/R9 chân 1** (đầu không nối +5) | **GND**: JP3 chân 7/8 hoặc JP2 chân 1 — **không phải GND12** |
| **PA10 / EN_CAN1** | **R2 chân 2** (phía MCU) | GND |
| **STB của TJA1044** | **R2 chân 1** = **B1 chân 8** | GND |
| Tham chiếu | U4 chân 8 (≈ 5 V), U3 chân 5 (≈ 3,3 V) | GND |

---

## 2. Các bài, theo thứ tự

| # | Làm gì | Mong đợi | Nếu khác |
|---|---|---|---|
| **0** | **Trước khi nạp.** Cấp +5 V. **Giữ SW1** (MCU ở reset, mọi chân thả nổi). Đo **PB6** và **STB** (mục 1.4). Thả SW1 | PB6 **≈ 5 V** ⇒ R8/R9 có lắp, **cửa sổ MAX485 phát lúc bật nguồn là có thật**. STB: ghi lại con số | PB6 ≈ 0 V ⇒ R8/R9 không lắp (DNP): ghi lại, cửa sổ đó không tồn tại trên board này. STB ≈ 0 V ⇒ trong lúc reset transceiver ở **Normal** — báo lại (datasheet TJA1044 không có trong repo; bài này đo thay) |
| **1** | Cấp +5 V, chưa nạp | **L1 sáng** (đèn nguồn, nối cứng). L2 tối hoặc theo firmware cũ | L1 tối ⇒ rail 3,3 V chết: kiểm D1, U3. Dừng |
| **2** | Nạp qua SWD (mục 1.3) | Nạp và verify thành công | "No target" ⇒ kiểm dây JP2, chân 4 |
| **3** | Nhìn L2 trong 10 s | **Nháy ngắn, đều, 2 lần/giây** — đếm **~20 lần / 10 s**. **Không bao giờ sáng liên tục** | **Sáng liên tục ⇒ rút nguồn ngay**, báo lại. **1 nháy rồi tối ~2 s, lặp** ⇒ mã 1 = `Error_Handler` (nghi Y1/HSE trước; đọc `s_detail` ở bài 3b). **Tối hẳn** ⇒ MCU không chạy hoặc HardFault |
| 3b | *(tuỳ chọn, debugger)* CubeIDE → Debug → Live Expressions | `SystemCoreClock` = `72000000`; `s_fault` = `APP_FAULT_NONE`; `s_count` = 0; `s_fw_id` = `"VF_TempHumi_Module phase 1"` | `SystemCoreClock` = 8000000 ⇒ đang chạy HSI, PLL chưa lên |
| **4** | CubeProgrammer → đọc bộ nhớ **`0x1FFFF7CC`**, độ rộng 16 bit | **`0x0040`** = 64 KB flash ⇒ đúng STM32F303**K8** | `0x0020` ⇒ chip K6 (32 KB): **dừng** — firmware và phân vùng bootloader không vừa |
| **5** | Đo **PB6** so GND khi firmware đang chạy | **≈ 0 V** (< 0,4 V) | ≈ 5 V ⇒ MAX485 đang phát và giữ bus RS485: **dừng, báo lại** |
| 5b | *(tuỳ chọn, oscilloscope)* Kênh 1 = +5 V (JP3-1), kênh 2 = PB6, trigger sườn lên kênh 1. Cấp nguồn | PB6 lên theo +5 V rồi **về 0 V sau vài ms** (LDO + POR + startup + `app_board_early_guard()`). Ghi độ rộng xung | Xung dài hàng chục–trăm ms ⇒ có gì đó chạy trước `USER CODE 1`: báo lại |
| **6** | Đo **PA10** (R2 chân 2) và **STB** (R2 chân 1) so GND | Cả hai **≥ 3 V** (HIGH = Standby) | < 0,8 V ⇒ transceiver ở Normal: **không cắm vào CAN1**, báo lại |
| **7** | Rút nguồn, chờ 5 s, cắm lại. ST-Link → **Connect**. Lặp **3 lần** | Kết nối được cả 3 lần — bằng chứng `.ioc` không tắt SWJ | Không kết nối được ⇒ thử cách giữ SW1 ở 1.3. Vẫn không ⇒ **dừng**: SWD là đường debug duy nhất |
| **8a** | **Bus cô lập:** analyzer + module + 120 Ω giữa CAN_H/CAN_L (JP3 chân 3/4 và 5/6), 500 kbit/s, analyzer ở chế độ **thường** (không phải listen-only). Nghe 60 s, trong lúc đó **rút–cắm nguồn module 5 lần** | **Không một frame nào, không error frame nào**, kể cả lúc cấp/ngắt nguồn | Có bất cứ gì ⇒ cổng im lặng đã hỏng: **dừng**, báo lại |
| **8b** | Vẫn bus cô lập đó. **Analyzer phát** một frame ID **`0x620`** (dải dự trữ, ID Map §1), DLC 8, mỗi 100 ms, trong 10 s | Analyzer báo **lỗi ACK / không có ACK**: frame không đi được, bộ đếm lỗi TX tăng, analyzer vào error-passive | Frame đi được (có ACK) ⇒ **module đang ACK** — bxCAN đã rời init mode hoặc transceiver ở Normal: **dừng**, báo lại |
| **8c** | **Bus CAN1 thật đang chạy** (mainboard + các module khác), analyzer nghe ≥ 60 s. Cắm module vào, rút ra, cắm lại | **Không một frame nào trong `0x400`–`0x49F`**. Bộ đếm lỗi bus không tăng khi cắm/rút. Mainboard không thấy node mới | như 8a |
| **9** | Cấp thêm **+12 V** (chung âm, mục 1.2). Cắm cảm biến vào J1. Rồi **rút J1 – chờ 5 s – cắm lại**, 3 lần | L2 **giữ nguyên** nhịp 2 Hz suốt bài. Không reset, không treo. Sau bài, ST-Link vẫn Connect được | L2 đổi nhịp, tắt, hay sáng ⇒ báo lại (giai đoạn 1 không có mã nào dính tới cảm biến) |
| — | Ghi dòng tiêu thụ | +5 V: ___ mA. +12 V có cảm biến: ___ mA | Chỉ là mốc so board, không có ngưỡng |

**Vì sao 8b mới là phép thử thật của B6 #3.** "Mainboard báo lỗi ACK" chỉ đúng khi module này là **node duy nhất khác** trên bus: chỉ cần một node nào khác ACK là người phát hài lòng, dù module này im lặng hay không. Ở 8b analyzer là node duy nhất còn lại, nên **không có ACK ⇔ module không ACK**.

*Nếu không có analyzer:* nối **chỉ mainboard + module** (cùng 120 Ω ở hai đầu). Mainboard phát GPS ở `0x600`–`0x60F` (ID Map §1); không ai ACK nên bộ đếm lỗi TX của mainboard phải tăng — xem được nếu firmware `VF_Main` có hiển thị trạng thái bus. Khôi phục bus đầy đủ ngay sau bài.

**Bài 0, 5 và 6 là lý do giai đoạn này tồn tại.** Bài 0 cho biết board thật có giống netlist không. Bài 5 và 6 đo trực tiếp hai chân giữ hai bus.

---

## 3. Bảng ghi kết quả — điền rồi gửi lại

| # | Kết quả (PASS / FAIL + ghi chú) |
|---|---|
| 0 | giữ SW1: PB6 = ___ V, STB = ___ V |
| 1 | |
| 2 | |
| 3 | đếm được: ___ lần / 10 s |
| 3b | |
| 4 | `0x1FFFF7CC` = 0x______ |
| 5 | PB6 = ___ V (U4 chân 8 = ___ V) |
| 5b | độ rộng xung PB6 lúc bật nguồn: ___ ms |
| 6 | PA10 = ___ V, STB = ___ V |
| 7 | |
| 8a | |
| 8b | analyzer báo: ________ |
| 8c | |
| 9 | |
| — | +5 V: ___ mA · +12 V có cảm biến: ___ mA |
| — | Trong hệ data logger, nguồn 12 V và 5 V có chung âm không? ________ |

---

## 4. Những gì quy trình này CHƯA kiểm được

- **Không có gì chạm vào cảm biến.** USART1 được khởi tạo nhưng không phát; giai đoạn 2 mới biết dây RS485, baud và địa chỉ có đúng không.
- **Đường `Error_Handler` / mã 1 chưa được kích hoạt thật.** Muốn thử: chập tạm OSC_IN xuống GND trên một board hỏng — **không bắt buộc**. Bài 3 đạt đã gián tiếp xác nhận HSE + PLL lên được.
- **Cửa sổ MAX485 phát lúc bật nguồn không thể đóng bằng firmware.** Bài 5b chỉ đo nó. Muốn xoá thì phải sửa phần cứng (R8/R9 xuống GND thay vì lên +5) ở lần ra PCB sau.
- **RS485 A/B lúc rảnh không phân biệt được "driver tắt" với "driver bật, DI = 1"**: cả hai đều cho A > B (DI có R7 kéo lên và TX của UART rảnh ở mức cao). Vì vậy bài 5 đo **chân PB6**, không đo A/B.
