# Giai đoạn 2 — Kế hoạch: driver RS485 + Modbus RTU

**Trạng thái: ĐÃ DUYỆT 2026-10-07** — cả ba câu hỏi ở mục 12 theo **phương án A**. Dụng cụ test: oscilloscope; nhiệt độ so tương đối với cảm biến phòng lab, không có ẩm kế tham chiếu.

| | |
|---|---|
| **Mục tiêu** | Đọc được nhiệt độ và độ ẩm từ cảm biến SHT qua RS485, mỗi 2 s, và **biết chính xác số đo cũ bao nhiêu** |
| **Kiểm được bằng** | Số đọc khớp với một ẩm kế đặt cạnh; oscilloscope cho thấy DE hạ **sau** bit stop cuối; rút dây thì bộ đếm lỗi tăng, tuổi số đo tăng, cắm lại thì hồi phục |
| **Không làm** | Không phát CAN (giai đoạn 3). Không ghi gì vào cảm biến. Mục 11 |

Chỗ đặt trong lộ trình: giai đoạn 1 đã chứng minh PB6 nhả bus (0 V khi chạy, 5 V khi reset). Giai đoạn này là lần đầu firmware **cầm** bus RS485 — và cầm nó đúng lúc, đủ lâu, không lâu hơn.

---

## 1. Dữ kiện đã kiểm trước khi lập kế hoạch

| Dữ kiện | Nguồn | Hệ quả |
|---|---|---|
| **Cả 11 khung ví dụ trong datasheet có CRC khớp CRC16 Modbus chuẩn** (đa thức 0xA001, khởi tạo 0xFFFF, byte thấp trước) | SHT datasheet tr. 7–11, kiểm bằng code trên host | 11 khung đó thành **vector test** — không phải số tự bịa |
| Đọc cả hai thanh ghi một lần: `01 03 00 00 00 02 C4 0B` → `01 03 04 01 02 FF 33 5B EA` = **25,8 %RH, −20,5 °C** | tr. 7 | Một giao dịch cho cả hai số ⇒ hai số cùng thời điểm. Có sẵn một trường hợp **nhiệt độ âm** để test |
| Nhiệt độ âm là **bù hai** (`0xFF33` = −205) | tr. 7, "Tính toán Nhiệt độ" | `int16_t`, không phải `uint16_t` |
| Địa chỉ `0xFF` đọc thanh ghi `0x07D0` thì cảm biến trả về **địa chỉ thật** của nó; đổi baud của master tới khi có trả lời | tr. 9 | Cơ sở cho phép quét chẩn đoán — câu hỏi 2 |
| MAX485: bật/tắt driver tZH, tZL, tLZ, tHZ **≤ 70 ns** | `MAX481.PDF` tr. 3 | So với một bit 208 µs ở 4800 baud: không cần chờ gì sau khi bật DE |
| MAX485: VIH của DE/RE̅ = 2,0 V | `MAX481.PDF` tr. 2 | PB6 push-pull 3,3 V bật được driver dù R8/R9 kéo lên 5 V |
| RE̅ nối chung DE | netlist `ENA_RS485` | Lúc phát, bộ thu **tắt**: không có tiếng vọng của chính mình. RO thả nổi, R10 kéo PB7 lên ⇒ UART thấy dòng rảnh |
| HAL F3 V1.5.7: `HAL_UART_Transmit_IT` gọi `HAL_UART_TxCpltCallback` ở cờ **TC**, không phải TXE | `stm32f3xx_hal_uart.c` (`UART_EndTransmit_IT`) | **Hạ DE ngay trong callback đó** là đúng lúc bit stop cuối rời dây — xem mục 4 |
| Receiver-timeout phần cứng (RTO): `stm32f303x8.h` có bit `RTOEN`/`RTOF` nhưng **không có** macro instance xác nhận cho USART1 trên F303x8, và RM0316 không có trong repo | header CMSIS | **Không dựa vào RTO.** Kết thúc khung bằng độ dài suy từ header + khoảng lặng đo bằng tick |
| Hardware DE của USART1 (`DEM`) nằm trên PA12 = CAN_TX | `VF_TH_IOC_Guide.md` §A6 | DE bằng phần mềm — như đã biết |

---

## 2. Kiến trúc — ba lớp, chỉ lớp dưới cùng biết HAL

```
app_th        lich hoi 2 s, may trang thai giao dich, goi drv_rs485       (HAL qua drv_rs485)
  |
  +-- app_meas   so do cuoi, tuoi, co, bo dem            THUAN - test tren host
  +-- drv_modbus CRC16, dung request, phan tich reply    THUAN - test tren host
  |
drv_rs485     USART1 + DE tren PB6, ngat, callback HAL                    (HAL)
```

Hai module thuần không include HAL, nên **toàn bộ phần dễ sai một cách im lặng** — CRC, phân tích khung, tuổi số đo, cờ, kế toán bộ đếm — chạy được trên PC với 11 vector của datasheet.

---

## 3. File và API

| File | Mới / sửa | Nội dung |
|---|---|---|
| `Core/{Inc,Src}/app/drv_rs485.*` | mới | Truyền bán song công. **Hàm bật DE là `static` ở đây, không export** (CLAUDE.md §7.1: không có hàm bật DE nào ngoài driver, và nó đi cùng TC). Sở hữu `HAL_UART_TxCpltCallback`, `HAL_UART_RxCpltCallback`, `HAL_UART_ErrorCallback` |
| `Core/{Inc,Src}/app/drv_modbus.*` | mới | `drv_modbus_crc16()`, `drv_modbus_build_read()`, `drv_modbus_parse_read()` → mã trạng thái cụ thể |
| `Core/{Inc,Src}/app/app_meas.*` | mới | Số đo cuối hợp lệ + thời điểm, `app_meas_age_x100ms()`, `app_meas_flags()`, bộ đếm theo loại |
| `Core/{Inc,Src}/app/app_th.*` | mới | Lịch 2 s, máy trạng thái giao dịch, (tuỳ câu hỏi 2) quét chẩn đoán |
| `Core/{Inc,Src}/app/app_diag.*` | mới | `g_diag` — "console" đọc bằng Live Expressions, như module góc. Magic `"TH02"` |
| `app_led.*` | sửa | Thêm mẫu **đang đo** (50 ms mỗi 5 s, 1%, như module góc) và — tuỳ câu hỏi 1 — mã **trạng thái** không chốt |
| `app_fault.h` | sửa | Thêm `APP_FAULT_SENSOR = 2` (trùng số với module góc) — ngữ nghĩa theo câu hỏi 1 |
| `app_config.h` | sửa | Độ dài khung, timeout, ngưỡng, bit `Flags` của proposal §5.1. `APP_FW_PHASE "2"` |
| `main.c` | sửa, chỉ USER CODE | `app_th_init()`, `app_th_task(now)`, `app_diag` |
| `tests/` | mới | `Makefile`, `test_drv_modbus.c`, `test_app_meas.c` — gcc MinGW + `make` của CubeIDE, như module góc |
| `docs/VF_TH_Phase2_Test.md`, `…_Report.md` | mới | sau khi code xong |

**Thêm file `.c` mới ⇒ bạn làm Refresh (F5) + Build Project một lần trong IDE** (CLAUDE.md §3).

API dự kiến (rút gọn):

```c
/* drv_rs485.h */
void            drv_rs485_init(void);
bool            drv_rs485_start(const uint8_t *req, uint8_t len);   /* false = busy / HAL error, counted */
drv_rs485_evt_t drv_rs485_poll(uint32_t now_ms, uint8_t *rx, uint8_t *rx_len);
void            drv_rs485_abort(void);                              /* DE low, RX/TX aborted */

/* drv_modbus.h - pure */
uint16_t      drv_modbus_crc16(const uint8_t *p, uint16_t n);
uint8_t       drv_modbus_build_read(uint8_t addr, uint16_t reg, uint16_t count, uint8_t out[8]);
mb_status_t   drv_modbus_parse_read(const uint8_t *p, uint8_t n, uint8_t addr, uint16_t count,
                                    uint16_t *regs, uint8_t *exception_code);
uint8_t       drv_modbus_expected_len(const uint8_t *p, uint8_t n);  /* 0 = not known yet */

/* app_meas.h - pure */
void     app_meas_record(app_meas_t *m, mb_status_t st, const uint16_t *regs, uint32_t now_ms);
uint16_t app_meas_age_x100ms(const app_meas_t *m, uint32_t now_ms);  /* 0xFFFF = never read */
uint8_t  app_meas_flags(const app_meas_t *m, uint32_t now_ms);       /* proposal 5.1 bits 0..4 */
```

---

## 4. Một giao dịch — và cái bẫy DE

```
IDLE --(2 s da qua)--> drv_rs485_start(01 03 00 00 00 02 C4 0B)
   1. xoa RX: flush RDR, xoa co ORE/FE/NE/PE  <- byte rac giua hai giao dich khong duoc
                                                  bien thanh "loi UART" o lan sau
   2. ARM nhan tung byte bang ngat            <- TRUOC khi phat: cam bien co the tra loi
                                                  ngay khi bit stop cuoi roi day
   3. DE = 1                                  <- MAX485 bat trong <= 70 ns
   4. HAL_UART_Transmit_IT(8 byte)
TX --(HAL_UART_TxCpltCallback = co TC, trong ngat)--> DE = 0, ghi tick
   --(qua MB_TX_TIMEOUT_MS ma chua co TC)--> DE = 0, huy, dem tx_timeout
RX --(du do dai suy tu header)------------> phan tich
   --(da co byte, lang > 3,5 ky tu)--------> phan tich (gan nhu chac la khung cut)
   --(khong byte nao trong 200 ms tu TC)---> dem timeout
   --(HAL_UART_ErrorCallback)--------------> DE = 0, dem uart_err
```

**Vì sao hạ DE trong `TxCpltCallback`:** `VF_TH_IOC_Guide.md` §A8 — `HAL_UART_Transmit()` trả về khi byte cuối vào thanh ghi dịch, **chưa ra khỏi dây**; hạ DE lúc đó cắt mất byte cuối, và ở 4800 baud cửa sổ sai rộng 2 ms. Ở chế độ ngắt, HAL chỉ gọi `TxCpltCallback` khi cờ **TC** lên — tức là đúng điều §A8 yêu cầu, **không cần vòng chờ nào**. Bài test oscilloscope (mục 10) đo đúng khoảng này.

**Vì sao có `MB_TX_TIMEOUT_MS`:** nếu TC không bao giờ tới (lỗi UART, HAL bị kẹt) thì DE nằm cao mãi và **chính module này giữ bus** — đúng cái lỗi giai đoạn 1 đã tốn công chặn. 8 byte mất 16,7 ms; timeout 50 ms. Hết hạn ⇒ DE thấp, huỷ, **đếm**.

**Kết thúc khung không dựa vào RTO phần cứng** (mục 1): sau byte thứ 2 biết mã hàm; `0x03` ⇒ chờ byte đếm rồi biết tổng `3 + n + 2`; `0x83` (exception) ⇒ 5 byte. Khoảng lặng > 3,5 ký tự (7,3 ms → 8 ms theo tick 1 ms) giữa chừng ⇒ khung cụt.

**Mọi lối ra của một giao dịch tăng đúng một bộ đếm.** Test host khẳng định: `started = ok + Σ lỗi + đang bay`. Một lối ra không đếm là một cách hỏng vô hình (CLAUDE.md §6.3).

| Bộ đếm | Khi nào |
|---|---|
| `ok` | khung đúng địa chỉ, đúng hàm, đúng byte đếm, CRC đúng |
| `timeout` | không một byte nào trong 200 ms |
| `crc_err` | đủ độ dài, CRC sai |
| `short_frame` | có byte nhưng lặng giữa chừng |
| `bad_addr`, `bad_func`, `bad_len` | khung CRC đúng nhưng không phải câu trả lời cho câu hỏi này |
| `exception` | cảm biến trả mã lỗi Modbus (giữ lại mã) |
| `uart_err` | PE / FE / NE / ORE |
| `tx_timeout`, `tx_fail` | TC không tới; HAL từ chối phát |

Ánh xạ sang hợp đồng ở giai đoạn 3: `ModbusOk` = `ok` (16 bit, vòng lại); `ModbusErr` = tổng lỗi (8 bit, **bão hoà ở 255**, proposal §5.2).

---

## 5. Tuổi số đo — làm ngay ở giai đoạn này, không đợi CAN

`DataAge` là lý do `TH_Data` tồn tại (CLAUDE.md §1.1). Nó được viết và test trên host **ở giai đoạn 2**, để giai đoạn 3 chỉ còn đóng gói.

| Quy tắc | Giá trị |
|---|---|
| Tính từ | **lần trả lời Modbus hợp lệ gần nhất** (proposal §5.1) — không phải lần hỏi, không phải lần phát CAN |
| Đơn vị | 0,1 s, `(now − t_ok) / 100` |
| Chưa đọc được lần nào | **`0xFFFF`** (proposal §5.1) |
| Bão hoà | **`0xFFFE`** = 6553,4 s — một số đo rất cũ **không bao giờ** được đọc ra thành "chưa đọc lần nào" |
| Tick vòng lại (49,7 ngày) | phép trừ không dấu — test host có ca vòng |

| Cờ (proposal §5.1) | Đặt khi |
|---|---|
| bit 0 `NEVER_READ` | chưa có lần đọc hợp lệ nào kể từ khởi động |
| bit 1 `TIMEOUT` | giao dịch **gần nhất** không có trả lời |
| bit 2 `CRC_ERR` | giao dịch gần nhất có trả lời nhưng CRC sai |
| bit 3 `STALE` | **đã từng** đọc được và tuổi > `APP_TH_STALE_MS` (6 s). Chưa đọc lần nào thì chỉ `NEVER_READ` — proposal tách hai lỗi này **có chủ ý** |
| bit 4 `RANGE` | số đo hợp lệ gần nhất ngoài dải datasheet — câu hỏi 3 |
| bit 5 `BUS_OFF_SEEN` | giai đoạn 3+ |

---

## 6. Giá trị đi thẳng, không đổi đơn vị

Thanh ghi → `int16_t t_x10`, `uint16_t rh_x10`, giữ nguyên ×10 (proposal §5.1: *"Một phép nhân chia ít đi là một chỗ sai ít đi"*). Không float, không lọc, không trung bình.

---

## 7. LED

| Mẫu | Khi nào |
|---|---|
| Khởi động, 2 Hz | từ lúc bật tới lần đọc hợp lệ đầu tiên |
| **Đang đo**, 50 ms mỗi 5 s (1%) | số đo hiện tại còn tươi — cùng mẫu với module góc |
| Mã 2 | cảm biến không trả lời — **ngữ nghĩa theo câu hỏi 1** |
| Mã 1 | `Error_Handler` — không đổi |

---

## 8. Test trên host

`tests/Makefile` theo mẫu module góc: `-std=c11 -Wall -Wextra -Werror -Wdouble-promotion -Wfloat-conversion`.

| Test | Ca |
|---|---|
| `test_drv_modbus` | **11 vector CRC của datasheet**; `build_read(1, 0, 2)` ra đúng 8 byte tr. 7; phân tích reply tr. 7 ra **258** và **−205**; exception `01 83 02 …`; CRC sai 1 bit; sai địa chỉ; sai hàm; byte đếm 2 thay vì 4; cụt ở mọi độ dài 0…8; `expected_len` ở từng byte |
| `test_app_meas` | tuổi: chưa đọc → `0xFFFF`; 0 ms → 0; 99 → 0; 100 → 1; bão hoà `0xFFFE`; tick vòng. Cờ: ranh giới STALE 6000 / 6001 ms; NEVER_READ không kéo theo STALE; TIMEOUT tự xoá khi giao dịch sau thành công. Kế toán: mọi `mb_status_t` tăng đúng một bộ đếm; `ModbusErr` bão hoà 255 |

---

## 9. Bộ nhớ

Ước lượng thêm 2–4 KB flash, < 200 B RAM. Vùng ứng dụng 52 KB đang dùng 30%. Không đáng lo; sẽ báo số thật.

---

## 10. Test trên board (phác thảo — bản đầy đủ viết sau khi code)

**Cần có:** cảm biến ở J1, **+12 V chung âm với +5 V** (CLAUDE.md §4.2), **oscilloscope 2 kênh**, **một ẩm kế/nhiệt kế tham chiếu**, debugger cho Live Expressions.

| # | Bài | Đạt khi |
|---|---|---|
| 1 | Đọc `g_diag` 1 phút | `ok` tăng 1 mỗi 2 s, mọi bộ đếm lỗi = 0, tuổi dao động 0…2 s, LED mẫu "đang đo" |
| 2 | So với tham chiếu đặt cạnh, chờ 10 phút cho cân bằng | lệch trong ±0,5 °C và ±4 %RH **cộng** sai số của tham chiếu (datasheet tr. 1) |
| 3 | Thở vào cảm biến / nắm trong tay | RH lên trong ≤ 8 s, T lên chậm hơn (≤ 25 s) — đúng thời gian đáp ứng tr. 1 |
| 4 | **Oscilloscope:** kênh 1 = A−B (hoặc PB7 / PA9), kênh 2 = PB6 | PB6 lên **trước** bit start đầu tiên; PB6 xuống **sau** bit stop của byte thứ 8, trong vòng < 1 bit (208 µs). Byte cuối nguyên vẹn |
| 5 | Rút J1 30 s rồi cắm lại | `timeout` tăng ~15; tuổi lên tới ~30 s; cờ STALE sau 6 s; cắm lại ⇒ hồi phục trong ≤ 2 s, tuổi về 0 |
| 6 | Tắt +12 V, bật lại sau 30 s (cảm biến khởi động **sau** MCU) | như bài 5 |
| 7 | Chạy 10 phút liên tục | **0 lỗi** trên ~300 giao dịch |
| 8 | Hồi quy giai đoạn 1 | CAN1 vẫn im lặng (8a/8b); SWD vẫn sống |

---

## 11. Cố tình KHÔNG làm

| | Vì sao |
|---|---|
| Không phát CAN | Giai đoạn 3. Trộn vào thì khi có lỗi không biết là Modbus hay CAN |
| **Không ghi bất cứ thanh ghi nào của cảm biến** (`0x06` tới `0x07D0`/`0x07D1`) | Đổi nhầm địa chỉ hay baud là "gạch hoá" cảm biến từ xa. Firmware chỉ đọc, mãi mãi — cấu hình cảm biến là việc của người, bằng công cụ trên PC |
| Không tự chấp nhận baud/địa chỉ tìm được khi quét (nếu có quét) | Một cảm biến bị cấu hình sai là một lỗi lắp đặt; tự thích nghi là giấu nó đi |
| Không lệnh `FORCE_READ` | Giai đoạn 6 (cần CAN). `app_th` sẽ có sẵn chỗ cho nó |
| Không float, không lọc | mục 6 |
| Không watchdog | Cần sửa `.ioc` ⇒ đề xuất ở giai đoạn 6 |

---

## 12. Cần bạn quyết

### Câu hỏi 1 — Cảm biến không trả lời thì LED nói gì?

Module góc **chốt** mã lỗi đầu tiên tới lúc reset. Nhưng cảm biến ở đây nằm trên **dây cắm rời và nguồn 12 V riêng** — rút ra cắm lại, hay bật 12 V sau MCU, là chuyện bình thường lúc lắp đặt.

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | Mã 2 là **trạng thái sống**, không chốt: hiện khi chưa đọc được lần nào sau 10 s, hoặc khi STALE; **tự tắt** khi đọc lại được. Mã lỗi chốt thật (mã 1) vẫn luôn thắng. Bộ đếm giữ lịch sử | Người lắp đặt cắm dây và **thấy ngay** LED chuyển sang "đang đo" trong ≤ 2 s. `app_led` thêm khái niệm "mã trạng thái" tách khỏi "mã lỗi chốt" |
| B | Như module góc: chốt mã 2 ở lần hỏng đầu tiên, tới lúc reset | Đơn giản, thống nhất. Nhưng bật 12 V chậm hơn MCU 10 s là LED báo lỗi mãi dù dữ liệu chạy đúng |

### Câu hỏi 2 — Không bao giờ đọc được thì có tự quét chẩn đoán không?

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | Sau 5 lần hỏi không trả lời (10 s), **một lần mỗi lần khởi động**: thử 4800 / 9600 / 2400 baud, mỗi baud hỏi `0xFF` đọc `0x07D0` (datasheet tr. 9). Tìm thấy ⇒ **chỉ ghi vào `g_diag`** ("cảm biến ở 9600, địa chỉ 5"), rồi quay về 4800 / địa chỉ 1. Không chấp nhận, không ghi | Phân biệt được ngay "dây hỏng" với "cảm biến bị cấu hình khác" — mà không cần một vòng nạp firmware nữa. Bài học 2-diag của module góc. Thêm ~1 KB |
| B | Không quét. Chỉ 4800 / địa chỉ 1 | Đơn giản hơn. Không trả lời thì phải mang cảm biến lên PC để tìm hiểu |

### Câu hỏi 3 — Số đo hợp lệ (CRC đúng) nhưng ngoài dải datasheet?

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | Giữ số, bật cờ `RANGE`, tuổi về 0 | Số trên bus khớp đúng thanh ghi của cảm biến; người phân tích thấy cờ và tự quyết. Đúng tinh thần proposal: cờ để báo, không để giấu |
| B | Bỏ số, đếm như lỗi, tuổi tiếp tục tăng | Một phép đo 60,5 °C thật trong lò thử sẽ trông như cảm biến chết |

### Không chặn, nhưng cần trước khi lắp đặt thật

Trong hệ data logger, nguồn 12 V cho JP2 và nguồn 5 V của harness CAN1 có chung âm không (CLAUDE.md §4.2). Giai đoạn 2 chạy trên bàn test với âm đã nối chung.
