# Giai đoạn 2 — Báo cáo

**Trạng thái: XONG 2026-10-07** — build sạch, test host 362/362, test board đạt (mục 8). Hai việc mở: độ chính xác độ ẩm (không có ẩm kế) và đo DE bằng oscilloscope (thay bằng phân tích, mục 8.3).

Firmware hỏi cảm biến SHT mỗi 2 s qua RS485 / Modbus RTU, giữ số đo hợp lệ cuối cùng **cùng tuổi của nó**, đếm mọi kết cục của mọi giao dịch, và nói tất cả qua `g_diag` (Live Expressions) và LED. Vẫn không phát CAN — đúng phạm vi `docs/VF_TH_Phase2_Plan.md`.

## 1. Kế hoạch đã duyệt và các quyết định

Kế hoạch duyệt 2026-10-07, cả ba câu hỏi theo **phương án A**:

| Câu hỏi | Quyết định | Ở đâu |
|---|---|---|
| 1. Cảm biến im lặng thì LED nói gì | **Mã 2 là trạng thái sống, không chốt**: hiện khi chưa đọc được gì sau 10 s, hoặc khi STALE; tự tắt khi đọc lại được. Mã lỗi chốt (mã 1) vẫn thắng | `app_th.c` cuối `app_th_task`, `app_led_set_status()` |
| 2. Quét chẩn đoán | Có — **một lần mỗi lần khởi động**, sau 5 lần hỏi không có số hợp lệ: 4800 / 9600 / 2400, đọc `0x07D0` ở địa chỉ `0xFF`. **Chỉ báo vào `g_diag.th.scan`, không bao giờ chấp nhận, không bao giờ ghi** | `app_th.c` `th_scan_*` |
| 3. Số đo ngoài dải | **Giữ số, bật `RANGE`**, tuổi về 0 | `app_meas_record()` |

Dụng cụ test (người dùng): oscilloscope; nhiệt độ so tương đối với cảm biến phòng lab; **không có ẩm kế tham chiếu**.

### 1.1 Một chỗ làm khác kế hoạch — STALE xét theo `DataAge`, không theo mili-giây

Kế hoạch §8 ghi ranh giới test "STALE 6000 / 6001 ms". Khi viết code thấy cách đó làm **log tự mâu thuẫn**: một reply cũ 6050 ms đi ra với `DataAge` = 60 (6,0 s) **và** cờ STALE, trong khi reply cũ 6000 ms cũng là `DataAge` = 60 **không** có cờ. Người đọc `.blf` thấy hai frame `6,0 s` ngược nhau.

Đã làm: `STALE ⇔ DataAge > 60` (> 6,0 s), xét trên chính con số được phát. Ranh giới thành **6099 ms tươi / 6100 ms STALE**. Test host khẳng định cả hai phía; một đột biến quay về so mili-giây thô thì test fail (mục 4.2).

### 1.2 Một chi tiết khác kế hoạch — magic của `g_diag`

Kế hoạch ghi `"TH02"`. Đã dùng **`"THD1"`** (`0x54484431`): magic đặt tên cho struct, không cho build — `"TH02"` sẽ đọc như "phase 2" và phải đổi mỗi giai đoạn.

## 2. File đã tạo / sửa

| File | Việc |
|---|---|
| `Core/{Inc,Src}/app/drv_rs485.*` | **Mới.** USART1 + MAX485 bán song công. **Hàm bật DE là `static`** (`rs485_drive_bus`), không export. DE hạ trong `HAL_UART_TxCpltCallback` = cờ TC. Xả RX + xoá cờ lỗi trước mỗi giao dịch, **bật nhận trước khi phát**. `drv_rs485_stop()` tắt ngắt quanh `HAL_UART_Abort` (đã đọc mã HAL: không chờ gì khi không có DMA). Sở hữu ba callback UART của HAL |
| `Core/{Inc,Src}/app/drv_modbus.*` | **Mới, thuần.** CRC16, dựng request 0x03, `expected_len` theo từng byte, phân tích reply theo thứ tự độ dài → CRC → địa chỉ → exception → hàm → byte đếm |
| `Core/{Inc,Src}/app/app_meas.*` | **Mới, thuần.** Số đo hợp lệ cuối, `DataAge`, cờ proposal §5.1, một bộ đếm cho mỗi kết cục, `ModbusOk` (16 bit, vòng) / `ModbusErr` (8 bit, bão hoà) |
| `Core/{Inc,Src}/app/app_th.*` | **Mới.** Lịch 2 s (lần đầu sau 2 s), máy trạng thái giao dịch không chặn, quét chẩn đoán, trạng thái LED |
| `Core/{Inc,Src}/app/app_diag.*` | **Mới.** `g_diag`, magic `THD1` |
| `app_led.*` | Mẫu **đang đo** (50 ms / 5 s, 1%) + **mã trạng thái sống** tách khỏi mã lỗi chốt. Duty của mẫu mới kiểm ở thì biên dịch |
| `app_fault.*` | `APP_FAULT_SENSOR = 2` — **không bao giờ** truyền vào `app_fault_raise()`; chỉ là số của mã LED. `APP_FAULT_DETAIL_UART_REINIT` cho lần đổi baud hỏng |
| `app_board.*` | `app_board_rs485_release()` (nhả bus: luôn an toàn, gọi từ ngắt). Trích dẫn cực tính DE từ datasheet |
| `app_config.h` | Khung, timeout, khoảng lặng, cờ, tuổi, LED, quét — mỗi hằng số datasheet ghi trang. 5 `_Static_assert` mới ở đây, thêm 4 trong `app_meas.c`. `APP_FW_PHASE "2"` |
| `main.c` | Chỉ USER CODE: `app_diag_init`, `app_th_init`, `app_th_task` |
| `tests/` | **Mới.** `Makefile`, `test_check.h`, `test_drv_modbus.c`, `test_app_meas.c` |
| `docs/VF_TH_Phase2_Test.md` | **Mới.** Quy trình test, chỉnh theo dụng cụ có thật |
| `.gitignore`, `CLAUDE.md`, `docs/VF_TH_Phase2_Plan.md` | `tests/*.exe`; cách chạy test host; trạng thái |

Không đụng `.ioc`, `Drivers/`, `contracts/`, `.cproject`, hay dòng nào ngoài USER CODE.

## 3. Một giao dịch, như code làm

```
app_th_task (moi vong lap)
  IDLE, den han ---------> th_start: app_meas_started, drv_rs485_send
                              xa RX, xoa ORE/FE/NE/PE, BAT NHAN, DE = 1, Transmit_IT
  TX ---(ngat TC)--------> HAL_UART_TxCpltCallback: DE = 0, ghi tick, sang RX
     ---(> 50 ms)--------> MB_ERR_TX      (DE ha cuong buc)
  RX ---(du expected_len)-> drv_modbus_parse_read -> MB_OK / CRC / ADDR / FUNC / LEN / EXCEPTION
     ---(lang > 8 ms)----> parse                  -> thuong la SHORT
     ---(0 byte, 200 ms)-> MB_ERR_TIMEOUT
     ---(ngat loi UART)--> MB_ERR_UART    (DE ha ngay trong ngat)
  moi loi ra -> th_finish -> drv_rs485_stop + app_meas_record: DUNG MOT bo dem
```

Đã kiểm trong image (`objdump`): lệnh đầu tiên của `HAL_UART_TxCpltCallback` là `bl app_board_rs485_release`; ba callback là symbol mạnh (`T`), không phải bản weak của HAL.

## 4. Test trên host

### 4.1 Kết quả

`cd tests && make` (gcc 6.3 MinGW, `-std=c11 -Wall -Wextra -Werror -Wconversion -Wdouble-promotion -Wfloat-conversion`):

```
test_drv_modbus: 193 checks, 0 failures
test_app_meas: 169 checks, 0 failures
```

| Test | Nội dung |
|---|---|
| `test_drv_modbus` | **11 khung của datasheet** (tr. 7–11), từng byte chép từ ảnh trang: CRC của cả 11; `build_read` ra đúng khung tr. 7 và tr. 9; reply tr. 7 ra **258** và **−205**; reply tr. 10 (địa chỉ 5) qua đường quét; cụt ở mọi độ dài 0…8; thừa 1 byte; **mọi phép lật 1 bit trong 72 bit** của reply tr. 7 đều bị bắt; sai địa chỉ; một thanh ghi thay vì hai; hàm 0x04; exception 0x83/02; exception cho hàm không hỏi; exception sai CRC |
| `test_app_meas` | Chưa đọc → `0xFFFF`, chỉ `NEVER_READ` (không kéo theo STALE); số tr. 7; tuổi 0 / 99 / 100 / 1999 ms; **bão hoà `0xFFFE`**; giao dịch hỏng không chạm số đo cũ; **tick vòng 2³²**; ranh giới STALE 6099 / 6100 ms; RANGE ở đúng mép −40,0 / +60,0 °C, 0 / 100 %RH và ngay ngoài; RANGE theo số đo chứ không theo giao dịch; **mỗi kết cục tăng đúng một bộ đếm** và `started = ok + lỗi` sau mỗi bước; trạng thái ngoài enum vẫn tăng một bộ đếm; `ModbusErr` bão hoà 255 (không phải 1255 & 0xFF = 231); `ModbusOk` vòng 16 bit |

### 4.2 Test có bắt được lỗi thật không — 5 đột biến

Cài từng lỗi vào bản sao nguồn (scratchpad), chạy lại test:

| Đột biến | Kết quả |
|---|---|
| CRC khởi tạo `0x0000` thay vì `0xFFFF` | 21 fail |
| Tuổi bão hoà ở `0xFFFF` (= "chưa đọc") | 1 fail |
| STALE so mili-giây thô (mục 1.1) | 2 fail |
| `ModbusErr` vòng thay vì bão hoà | 1 fail |
| Nhiệt độ mất dấu | 4 fail |

## 5. Build và bộ nhớ

**Build trong IDE** (người dùng, 10:50:03): `0 errors, 0 warnings`. **Rebuild sạch headless**: 0 error, 0 warning, cùng con số. Các file `app_*`/`drv_*` sạch thêm với `-Wextra -Wconversion -Wshadow -Wdouble-promotion` ở cả `-O0` và `-Os`. **Không có hàm `double` nào trong image** (`nm`: 0 symbol `__aeabi_d*` / `__*df3`).

```
   text    data     bss     dec     hex  filename
  22936      12    2036   24984    6198  VF_TempHumi_Module.elf      (Debug, -O0)
```

| Tài nguyên | GĐ 1 | GĐ 2 | Có | Tỷ lệ |
|---|---|---|---|---|
| Flash (text + data) | 15 920 | **22 948** | 53 248 (vùng ứng dụng sau phase 5) | **43,1%** |
| RAM tĩnh (data + bss, không kể heap/stack) | 244 | **512** | 12 288 | 4,2% |

**Flash tăng 7 KB, kế hoạch ước 2–4 KB — ước thấp.** Tách ra: **~2,6 KB** là bộ máy UART ngắt của HAL vừa được kéo vào (`HAL_UART_Abort` 498, `UART_RxISR_8BIT` / `_16BIT` 424 + 424 — HAL chọn theo độ dài từ lúc chạy nên cả hai được link —, `UART_Start_Receive_IT` 396, `Transmit_IT`, `TxISR`…); **~4,4 KB** là code ứng dụng ở `-O0`: `app_th` 1 846, `drv_rs485` 992, `drv_modbus` 804, `app_led` 792, `app_meas` 700. Vẫn còn 57% vùng ứng dụng.

RAM: `g_diag` 116 B, `app_th` 90 B, `drv_rs485` 49 B.

### 5.1 Stack (`-fstack-usage`, `-O0`)

| Đường | Ước lượng |
|---|---|
| Luồng chính, sâu nhất (quét → đổi baud): `main` 16 → `app_th_task` 24 → `th_service` 72 → `th_parse_and_finish` 32 → `th_finish` 32 → `th_scan_record` 32 → `th_scan_finish` 24 → `drv_rs485_set_baud` 16 → `HAL_UART_Init` 16 → `UART_SetConfig` 40 | ~320 B |
| Ngắt USART1 chồng lên: `HAL_UART_IRQHandler` **240** → `UART_RxISR_8BIT` 120 → `RxCpltCallback` 16 → `UART_Start_Receive_IT` 96 | ~480 B |
| Khung ngắt có ngữ cảnh FPU | ~100 B |

Tổng xấu nhất **~0,9–1 KB**, ngang mức tối thiểu 1 KB (`_Min_Stack_Size`) mà linker kiểm. **Không phải rủi ro thật**: stack mọc xuống vùng RAM không dùng, còn ~11,7 KB (RAM tĩnh 512 B, heap không dùng). Không có ngắt lồng nhau (USART1 và CAN cùng ưu tiên 0, SysTick 15). Ghi lại để biết: ở `-O0`, HAL tốn stack nhiều hơn code ứng dụng.

## 6. Quy trình test trên phần cứng

Ở **`docs/VF_TH_Phase2_Test.md`**. Tóm tắt:

| # | Kiểm | Đạt khi |
|---|---|---|
| 1 | Chạy 1 phút | `count[0]` +1 mỗi 2 s, mọi lỗi = 0, LED 1 nháy / 5 s |
| 2 | T so với cảm biến phòng lab, sau 15 phút | chênh lệch ổn định, \|Δ\| ≈ ±0,5 °C + sai số cảm biến phòng |
| 3 | RH để yên; thở vào | hợp lý, ổn định; tăng rõ trong ≤ 8 s rồi về |
| **4** | **Oscilloscope PB6 + PA9** | **PB6 xuống ~208 µs sau sườn lên cuối của PA9** (bit stop của `0x0B`) |
| 4b | PB6 + PB7 | 9 byte trả lời sau khi PB6 xuống, không gì trong lúc PB6 cao |
| 5, 7 | Rút J1 / tắt 12 V 30 s | TIMEOUT, rồi STALE + mã 2 sau 6,1 s; hồi phục ≤ 2 s; số cũ giữ nguyên, tuổi tăng |
| 6 | Khởi động không cảm biến | mã 2 sau 10 s; quét chạy, `status` = {1,1,1}, baud về 4800 |
| 8 | 10 phút | ~300 OK, 0 lỗi |
| 9 | Hồi quy GĐ 1 | CAN1 im lặng, SWD sống |
| 10, 11 | *(tuỳ chọn)* đảo A/B; cảm biến ở 9600 | lỗi được đếm; quét tìm ra 9600 mà không chấp nhận |

## 7. Vấn đề còn mở

1. ~~**Chưa test trên board.**~~ Đã test 2026-10-07 — mục 8.
2. **Thời gian trả lời của cảm biến chưa biết** — datasheet không ghi. `MB_REPLY_TIMEOUT_MS` = 200 ms là lựa chọn của firmware. Bài 1 / 8 đo `latency_ms` / `latency_max_ms`; nếu max gần 200 ms thì phải xem lại.
3. **Độ ẩm chưa có phép kiểm độ chính xác** (không có ẩm kế). Nếu sau này có, chạy lại bài 2 cho RH.
4. **Đường `MB_ERR_TX` và `MB_ERR_EXCEPTION` chưa từng chạy trên phần cứng** — chỉ host test (exception) và đọc mã (TX timeout).
5. **`HAL_UART_Init` khi đổi baud** gọi `UART_CheckIdleState`, chờ TEACK/REACK với timeout `HAL_UART_TIMEOUT_VALUE` = 0x1FFFFFF ms của HAL. Hai cờ đó lên sau vài chu kỳ, nên thực tế không chờ; nhưng giới hạn trên là của HAL, không phải của firmware này. Chỉ chạy trong quét chẩn đoán (tối đa một lần mỗi lần khởi động).
6. **Các việc mở từ giai đoạn 1 vẫn còn** — CLAUDE.md §7.2: chung âm 12 V / 5 V trong hệ data logger; proposal TH chưa có nhà ở `VF_Main`; hai lỗi chữ trong hợp đồng; watchdog.

# 8. Kết quả test trên board — đang chạy

Image `a7717c2`. Người dùng chạy `docs/VF_TH_Phase2_Test.md`.

## 8.1 Lần chạy đầu: chưa cấp 12 V — cảm biến im lặng (2026-10-07)

Chạy bài 1 khi **board chưa có +12 V** (cảm biến không có nguồn). `g_diag` sau 5 lần hỏi:

| Trường | Giá trị | Đúng thiết kế? |
|---|---|---|
| `magic` | 1414022193 = `0x54484431` "THD1" | ✓ đúng struct |
| `fw_version` / `fw_phase` | 1 / 2 | ✓ |
| `started` | 5 | |
| `count[1]` TIMEOUT | 5; mọi `count` khác 0 | ✓ **`started = ok + lỗi` = 0 + 5** |
| `last_rx_len` | 0 | ✓ không một byte nào, kể cả rác (`count[8]` = 0) |
| `flags` | 3 = NEVER_READ + TIMEOUT | ✓ không kéo theo STALE |
| `age_x100ms` | 65535 | ✓ "chưa đọc lần nào" |
| `modbus_err` | 5 | ✓ |
| `baud` | 4800 | ✓ |
| LED | 2 Hz trong ~8–10 s đầu, rồi 2 nháy mỗi ~2 s | ✓ **mã 2 sau 10 s**, sống |

Không phải lỗi firmware — nhưng là **bằng chứng đầu tiên trên phần cứng** rằng trường hợp "cảm biến im lặng" được báo đúng ở mọi kênh: bộ đếm, cờ, tuổi, LED. Đó chính là phần nửa đầu của bài 6. `scan` không được đọc ở lần này (ảnh chụp lúc `started` = 5, quét chạy ở ~12 s).

Bài học, đã thêm vào quy trình (bài 0): **đo 12 V ở J1 và chung âm trước bài 1.** Module cần hai nguồn (CLAUDE.md §4.2) và thiếu nguồn thứ hai trông giống hệt một lỗi dây.

## 8.2 Có 12 V — mọi bài

| # | Bài | Kết quả |
|---|---|---|
| 1 | Chạy 1 phút | **PASS** — `count[0]` tăng, mọi `count[1..9]` = 0, LED 1 nháy / 5 s. `latency_ms`: không báo số |
| 2 | T so với phòng lab | **PASS (tương đối)** — `t_x10` = **254** (25,4 °C), phòng ~25 °C: Δ ≈ +0,4 °C, trong ±0,5 °C của datasheet tr. 1 |
| 3 | RH để yên; thở vào | **PASS (độ hợp lý)** — `rh_x10` = **510** (51,0 %RH), hợp lý cho phòng lab; thở vào thì tăng dần đều. **Không phải phép kiểm độ chính xác** — không có ẩm kế tham chiếu |
| 3b | Nắm tay | **PASS** — T tăng dần đều lên ~28–30 °C |
| 4, 4b | Oscilloscope DE | **KHÔNG ĐO** — oscilloscope chưa sẵn sàng. Người dùng chấp nhận thay bằng phân tích: mục 8.3 |
| 5 | Rút J1 30 s, cắm lại | **PASS** — `flags` = **2** (TIMEOUT) ngay sau lần hỏi kế; khi `age` > 60: `flags` = **10** (TIMEOUT + STALE), LED mã 2; `count[1]` +~15. Cắm lại: `flags` = 0, `count[0]` tăng, `age` về 0 rồi đếm tiếp, LED về 1 nháy / 5 s. Số đo **giữ nguyên số cũ** suốt lúc rút — tuổi là thứ nói nó cũ |
| 6 | Khởi động không cảm biến | **PASS** — `flags` = 3, `age` = 65535, LED mã 2; `scan.ran` = 1, `done` = 1, `status` = {1, 1, 1}. Cắm cảm biến ⇒ đọc lại được (nên baud đã về 4800) |
| 7 | Tắt 12 V 30 s | **PASS** |
| 8 | 10 phút liên tục | **PASS** — 0 lỗi (≈ 300 giao dịch). `latency_max_ms`: không báo số |
| 9 | Hồi quy GĐ 1 | **PASS** — CAN1 im lặng, SWD sống |
| 10 | Đảo A/B | không chạy (tuỳ chọn) — dây giữ đúng datasheet tr. 2 |
| 11 | Cảm biến đặt 9600 qua PC | **PASS** — quét tìm ra 9600 **mà không chấp nhận**: đường "found" của quét đã chạy trên phần cứng thật |

## 8.3 Bài 4 thay bằng phân tích — DE hạ đúng lúc

Bài 4 kiểm hai điều: DE **không hạ sớm** (cắt byte cuối) và **không hạ muộn** (đụng câu trả lời).

**Không thể sớm — do cấu trúc, đã xác minh trong mã:** DE chỉ hạ ở ba chỗ: `HAL_UART_TxCpltCallback`, `HAL_UART_ErrorCallback`, `drv_rs485_stop()`. HAL chỉ gọi `HAL_UART_TxCpltCallback` từ `UART_EndTransmit_IT`, và chỉ khi `ISR.TC` và `CR1.TCIE` cùng bật (`stm32f3xx_hal_uart.c:2404` → `:3819`). TC là cờ phần cứng bật **sau bit stop của byte cuối**. Lỗi UART không xảy ra trong lúc phát (RE̅ = DE ⇒ bộ thu tắt). `drv_rs485_stop()` chỉ chạy sau khi giao dịch đã kết thúc hoặc sau 50 ms timeout.

**Không thể muộn — ước lượng trên image thật** (`objdump` của `a7717c2`, `-O0`):

| Đường từ TC tới ghi BSRR | Số lệnh (tĩnh) |
|---|---|
| `USART1_IRQHandler` | 8 |
| `HAL_UART_IRQHandler` tới lệnh gọi `UART_EndTransmit_IT` | ≤ 543 (cận trên: coi như chạy hết mọi nhánh) |
| `UART_EndTransmit_IT` → `HAL_UART_TxCpltCallback` → `app_board_rs485_release` → `HAL_GPIO_WritePin` | 38 + 25 + 10 + 23 |
| **Tổng, cận trên** | **≈ 650 lệnh** |

≈ 650 lệnh × ~2 chu kỳ (flash 2 wait-state, prefetch bật) ≈ 1 300 chu kỳ ≈ **18 µs** ở 72 MHz, cộng tối đa vài µs nếu ngắt rơi vào một vùng tắt ngắt ngắn của vòng lặp chính (`drv_rs485_snapshot`). **DE hạ muộn nhất ~25–30 µs sau bit stop — dưới 15% một bit (208 µs).** Một slave Modbus RTU chỉ coi khung đã kết thúc sau **3,5 ký tự lặng = 7,3 ms**, nên không thể trả lời sớm hơn thế: biên **> 250 lần**.

**Bằng chứng thực nghiệm khớp với phân tích:** bài 8 — ≈ 300 giao dịch, **0 lỗi**. Cảm biến chỉ trả lời request có CRC đúng, và byte cuối của request là byte CRC cao (`0x0B`): mỗi lần trả lời là một lần byte cuối đã ra dây nguyên vẹn. Không một `CRC_ERR`, `SHORT` hay `UART` nào ⇒ câu trả lời chưa lần nào đụng driver của chính module.

**Còn mở:** đo thật bằng oscilloscope khi có (bài 4 / 4b giữ nguyên trong quy trình). Không chặn giai đoạn 3.

## 8.4 Tiêu chí của kế hoạch

| Kế hoạch | Kết quả |
|---|---|
| Số đọc khớp tham chiếu | Nhiệt độ **đạt (tương đối)**, Δ ≈ +0,4 °C. Độ ẩm: **chỉ kiểm được độ hợp lý và phản ứng** — không có tham chiếu |
| DE hạ sau bit stop | **phân tích + thực nghiệm gián tiếp** (8.3); chưa đo bằng oscilloscope |
| Rút dây ⇒ lỗi tăng, tuổi tăng; cắm lại ⇒ hồi phục | **đạt** (bài 5, 6, 7) |
| 0 lỗi trong 10 phút | **đạt** (bài 8) |
| Hồi quy GĐ 1 | **đạt** (bài 9) |
| Build 0/0, test host | **đạt** — 0/0, 362/362 |

**Giai đoạn 2 khép lại 2026-10-07**, với hai việc mở ghi rõ ở trên: độ chính xác độ ẩm (cần ẩm kế) và đo DE bằng oscilloscope. Số `latency_ms` / `latency_max_ms` người dùng không báo; 0 timeout trên ~300 giao dịch chứng minh chúng **dưới 200 ms**, nhưng con số thật vẫn chưa biết — đọc ở lần debug tới.
