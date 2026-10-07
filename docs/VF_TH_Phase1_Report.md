# Giai đoạn 1 — Báo cáo

**Trạng thái: XONG 2026-10-07** — build sạch 2026-10-05, test board đạt mọi bài bắt buộc 2026-10-07 (mục 8).

Khung project chạy bare-metal: nhả bus RS485 trước mọi thứ, im lặng trên CAN1, báo hiệu bằng LED L2. Giai đoạn này không nói chuyện với cảm biến, không phát CAN, không có bootloader — đúng phạm vi `docs/VF_TH_Phase1_Prompt.md`.

## 1. Điều kiện tiên quyết — cả 6 đạt

| # | Điều kiện | Kết quả |
|---|---|---|
| 1 | Proposal đã duyệt, câu hỏi §4 có trả lời | "**ĐÃ DUYỆT 2026-10-05, theo PHƯƠNG ÁN A**". Bản ở `docs/` và `contracts/` giống từng byte |
| 2 | Chỉ có DBC v1.5 | `contracts/` không có v1.4; `check_contracts: ok, 4 files` |
| 3 | ID Map v1.3 | "v1.3 — ĐÃ DUYỆT 2026-10-05" |
| 4 | Có `.ioc` | `VF_TempHumi_Module.ioc` (CubeMX 6.18.1, FW_F3 V1.11.6) |
| 5 | Không tắt SWD | `grep -r SWJ Core/` rỗng; `.ioc`: PA13/PA14 = `Serial_Wire` |
| 6 | Pinout khớp §4.1 | `usart.c`: PA9 + PB7 (AF7). `gpio.c`: PA10 SET, PB6 RESET, PB0 SET. **Đối chiếu thêm với database của CubeMX** (`db/mcu/STM32F303K(6-8)Tx.xml`): pin 14 = PB0, 19 = PA9, 20 = PA10, 21 = PA11, 22 = PA12, 23 = PA13, 24 = PA14, 29 = PB6, 30 = PB7 — khớp hết |

Checklist §A11 của IOC Guide cũng đạt: `RCC_PLL_MUL9`, `RCC_HCLK_DIV2`; CAN 4 / 15 / 2 / 1; NVIC `CAN_RX0`, `CAN_SCE`, `USART1`.

Mọi hằng số cảm biến trong B2 đã đối chiếu với **ảnh trang** datasheet SHT (trang 1, 4, 5, 6): đúng hết.

## 2. File đã tạo / sửa

| File | Việc |
|---|---|
| `.gitattributes` | **Mới.** Ghim LF cho `*.sh`, `makefile.init`, `contracts/*`. Máy này `core.autocrlf=true`; thiếu file này thì một bản clone mới làm lệch cả bốn hash dù nội dung giống từng byte |
| `makefile.init` | **Mới.** Chạy `tools/check_contracts.sh` ở đầu mỗi lần `make`, `$(error)` nếu lệch. Cùng cơ chế với hai module anh em |
| `Core/Inc/app/app_config.h` | **Mới.** Mọi hằng số, hằng số datasheet ghi trang + mục. Dung lượng MCU đã xác nhận (mục 5). 18 `_Static_assert` khoá macro vào bảng hợp đồng |
| `Core/{Inc,Src}/app/app_board.*` | **Mới.** `app_board_early_guard()` (trước `HAL_Init`), trạng thái an toàn PB6 LOW → PA10 HIGH → LED tắt. **Cố ý không có** hàm đưa transceiver sang Normal hay bật DE |
| `Core/{Inc,Src}/app/app_led.*` | **Mới.** Mẫu khởi động, mã lỗi, đường nháy không cần ngắt cho `Error_Handler`. Duty > 10% là lỗi biên dịch. Cùng ngôn ngữ LED với module góc |
| `Core/{Inc,Src}/app/app_fault.*` | **Mới.** Chốt lỗi đầu tiên, đếm mọi lỗi, an toàn từ ISR. `app_fault_fatal()` cho đường không quay lại |
| `Core/Src/main.c` | Chỉ trong USER CODE: include, `s_fw_id`, early guard (`USER CODE 1`), trạng thái an toàn (`USER CODE 2`), super-loop, `Error_Handler` |
| `Core/Src/stm32f3xx_it.c` | Chỉ trong USER CODE: HardFault về trạng thái an toàn trước khi treo |
| `contracts/README.md` | **Viết lại.** Bản cũ chép nguyên từ module góc (nói về `VF_Angle_CAN_Proposal.md`, v1.2, `node_id 0x20–0x29`). Script không băm file này. Người dùng duyệt sửa 2026-10-05 |
| `docs/VF_TH_Phase1_Test.md` | **Mới.** Quy trình test trên board |
| `CLAUDE.md` | §3 cách build; §4.2 `GND12` ≠ `GND`, +12 V cạnh SWD; §4.3 early guard; §7 trạng thái |

Không đụng `.ioc`, `Drivers/`, file hợp đồng nào trong `contracts/`, `.cproject`, hay dòng nào ngoài USER CODE.

## 3. Những quyết định của giai đoạn này

### 3.1 Thứ tự khởi động — lệch khỏi B3, đã duyệt

```
Reset_Handler -> SystemInit (chi bat FPU) -> copy .data / zero .bss -> main()
  USER CODE 1   app_board_early_guard()      PB6 = LOW, PA10 = HIGH      <- MOI
  HAL_Init()
  SystemClock_Config()                        HSE cho toi 100 ms; hong -> Error_Handler
  MX_GPIO_Init / MX_CAN_Init / MX_USART1_UART_Init
  USER CODE 2   app_board_enter_safe_state() PB6, PA10, LED - B3, khong tin .ioc
                app_led_init()
  while (1)     app_led_task()
```

B3 đặt PB6 LOW ở `USER CODE 2`. Lúc đó `MX_GPIO_Init` đã làm rồi — và cả hai đều nằm **sau** `SystemClock_Config`, hàm có thể chờ HSE tới `HSE_STARTUP_TIMEOUT` = 100 ms, và **nếu thạch anh chết thì nhảy thẳng vào `Error_Handler` trước khi `MX_GPIO_Init` chạy**: PB6 không bao giờ được cấu hình, MAX485 giữ bus mãi.

`app_board_early_guard()` tự bật clock GPIOA/B, **ghi ODR trước rồi mới chuyển MODER sang output**, nên PB6 đi thẳng từ "bị kéo lên +5 V" sang "kéo xuống" mà không có khoảnh khắc nào bị lái lên cao. Không phụ thuộc SysTick. Đã kiểm trong `.list`: `bl app_board_early_guard` là lệnh gọi đầu tiên của `main()`, trước `bl HAL_Init`.

B3 vẫn giữ nguyên ở `USER CODE 2`, để một lần Generate quên PinState không lọt qua được.

**Phần còn lại của cửa sổ** (reset → startup → `main`) firmware không xoá được. Bài 5b của quy trình test đo nó.

### 3.2 Ngôn ngữ LED (L2, PB0, tích cực LOW)

Cùng mẫu và cùng con số với `VF_Angle_Module`, để kỹ thuật viên đọc mọi loại module như nhau.

| Mẫu | Trông thế nào | Duty | Nghĩa |
|---|---|---|---|
| **Khởi động** | nháy 50 ms, đều 2 lần/giây | 10,0% | firmware chạy, chưa đo. Giai đoạn 1 chỉ có mẫu này |
| **Mã N** | N lần nháy 50 ms cách nhau 250 ms, rồi tối 2 s, lặp | ≤ 9,6% (mã 8) | lỗi N đã chốt |
| **Tối hẳn** (L1 vẫn sáng) | — | 0 | MCU không chạy: chip trắng, HardFault |
| **Sáng liên tục** | — | 100% | **không được xảy ra** — rút nguồn |

### 3.3 Mã lỗi

| Mã | Tên | Khi nào |
|---|---|---|
| 1 | `APP_FAULT_HAL_INIT` | `SystemClock_Config` hoặc một `MX_*_Init` trả lỗi → `Error_Handler`. Detail = địa chỉ trả về |

Dự kiến, **chưa định nghĩa**: 2 = cảm biến không trả lời (giai đoạn 2), 3 = config sai CRC (giai đoạn 4) — **trùng số với module góc** ở chỗ nghĩa trùng. Một mã chỉ tồn tại khi có code phát được nó.

### 3.4 `Error_Handler`

Như module góc: lấy LR → tắt ngắt → `MX_GPIO_Init()` lại (nếu chính `SystemClock_Config` hỏng thì chân LED còn là input) → `app_fault_fatal(HAL_INIT, LR)` → trạng thái an toàn → nháy **mã 1** mãi mãi, nhịp lấy từ `SysTick COUNTFLAG` (vẫn chạy khi ngắt tắt). Mọi vòng chờ có giới hạn (§6.3).

### 3.5 Im lặng trên CAN1 — hai lớp

- PA10 = HIGH ⇒ TJA1044 ở Standby. Đặt ở `USER CODE 1`, bởi `.ioc`, và lại ở `USER CODE 2`.
- `MX_CAN_Init` chạy nhưng **không ai gọi `HAL_CAN_Start`** ⇒ bxCAN nằm ở init mode, CANTX lặn.

## 4. `check_contracts` trong build — đã chứng minh fail được

`Debug/makefile` do CubeIDE sinh mở đầu bằng `-include ../makefile.init` ⇒ không sửa dòng nào của file sinh tự động, nên không phải dừng theo §6.1. Build trong IDE in `check_contracts: ok, 4 files match VF_Main.` **trước** dòng biên dịch đầu tiên.

**Phép thử 1 — sửa một byte** (`0x49F` → `0x49E` ở dòng 5 của `VF_Main_CAN_ID_Map.md`; `cmp -l` xác nhận đúng 1 byte khác, offset 412, `F` → `E`):

```
check_contracts: FAILED - a contract file no longer matches.

  VF_Main_CAN_ID_Map.md: FAILED
  sha256sum: WARNING: 1 of 4 computed checksums did NOT match

  These files belong to VF_Main. One of two things happened:
  ...
  Do NOT run --update just to clear this. The point of the check is
  that a layout change reaches the firmware, not that the build is green.
../makefile.init:33: *** contracts/ no longer matches VF_Main - see the report above. Fix the mismatch; running check_contracts.sh --update just to clear this defeats the point.  Stop.
make exit code: 2
```

**Phép thử 2 — sót một DBC cũ** (chép `VF_Main/dbc/VF_E-Drive_Data_Logger_v1.4.dbc` vào `contracts/`), chạy với **PATH chỉ có tool của ST** (như khi build trong IDE — `sh` là BusyBox v1.31.0.st của plugin make, có `sha256sum`):

```
check_contracts: FAILED - the set of contract files changed.
  in contracts/ :
    VF_E-Drive_Data_Logger_v1.4.dbc
    VF_E-Drive_Data_Logger_v1.5.dbc
    VF_Main_CAN2_Service_Protocol.md
    VF_Main_CAN_ID_Map.md
    VF_TH_CAN_Proposal.md
  in SHA256SUMS:
    VF_E-Drive_Data_Logger_v1.5.dbc
    VF_Main_CAN2_Service_Protocol.md
    VF_Main_CAN_ID_Map.md
    VF_TH_CAN_Proposal.md
../makefile.init:33: *** contracts/ no longer matches VF_Main ...  Stop.
make exit code: 2
```

Build dừng **trước khi biên dịch file đầu tiên**. `make clean` trong lúc lệch: **exit 0** (có chủ đích — quay về trạng thái biết rõ không bao giờ được bị chặn).

Đã hoàn nguyên cả hai: `cmp` giống hệt bản lưu, `check_contracts: ok, 4 files match VF_Main.`, rebuild sạch, `git status contracts/` chỉ còn `README.md` (sửa có chủ đích).

## 5. Build và bộ nhớ

**Build trong IDE** (người dùng, 22:00:17): `0 errors, 0 warnings`. **Rebuild sạch headless** (`make clean && make all`, cùng toolchain GNU Tools for STM32 14.3.rel1): 0 error, 0 warning, cùng con số.

Kiểm thêm các file `app_*` với `-Wextra -Wconversion -Wshadow -Wdouble-promotion`, ở cả `-O0` và `-Os`, và cả `-std=c11`: sạch.

```
   text    data     bss     dec     hex  filename
  15908      12    1772   17692    451c  VF_TempHumi_Module.elf      (Debug, -O0)
```

### 5.1 Dung lượng thật của MCU — đã xác nhận

| Nguồn | Flash | SRAM | CCM RAM |
|---|---|---|---|
| CubeMX database `STM32F303K(6-8)Tx.xml` | 32 (K6) / **64 (K8)** KB | **12** KB | **4** KB |
| `STM32F303K8TX_FLASH.ld` (CubeMX sinh) | 64K @ `0x08000000` | 12K @ `0x20000000` | 4K @ `0x10000000` |
| Trên chip (bài 4 của quy trình test) | `0x1FFFF7CC` phải đọc `0x0040` | | |

Ghi vào `app_config.h` (`APP_MCU_FLASH_KB` …) kèm nguồn. Con số "64 KB" của IOC Guide §A9 đúng, nhưng giờ nó có nguồn.

### 5.2 Dùng bao nhiêu

| Tài nguyên | Dùng | Có | Tỷ lệ |
|---|---|---|---|
| Flash | 15 920 B (text + data) | 65 536 B | 24,3% |
| Flash, so với **vùng ứng dụng sau phase 5** | 15 920 B | 53 248 B (52 KB) | **29,9%** |
| RAM | 1 784 B | 12 288 B | 14,5% |

- RAM: `._user_heap_stack` **1 540 B** (heap 0x200 + stack 0x400 do linker giữ chỗ) + `.bss` 232 B + `.data` 12 B. Trong 244 B tĩnh thật: handle HAL `huart1` 136 B + `hcan` 40 B; ba module `app_*` thêm **21 B**.
- Flash: ba module `app_*` **1 080 B**; `main` 68 B, `Error_Handler` 26 B, `s_fw_id` 27 B, HardFault 12 B. Phần lớn còn lại là HAL ở `-O0` (`HAL_RCC_OscConfig` một mình 4 124 B, `HAL_UART_IRQHandler` 1 484 B).
- Chưa build Release (`-Os`): CubeIDE chưa sinh `Release/makefile`. Không chặn gì — với 30% vùng ứng dụng ở `-O0`, còn rất rộng.

### 5.3 Stack (`-fstack-usage`)

Chuỗi sâu nhất trong code ứng dụng là đường fatal: `Error_Handler` 16 → `app_fault_fatal` 16 → `app_led_code_forever` 24 → `led_poll_wait_ms` 32 = **88 B**. Đường thường: `main` 16 → `app_led_task` 24 → `led_enter_phase` 24 → `app_board_led_set` 16. Stack giữ chỗ 1 KB.

## 6. Quy trình test trên phần cứng

Ở **`docs/VF_TH_Phase1_Test.md`**. Tóm tắt các bài:

| # | Kiểm | Vì sao |
|---|---|---|
| 0 | Giữ SW1, đo PB6 và STB | **Sự thật phần cứng mà netlist không cho được**: R8/R9 có lắp không (≈ 5 V), TJA1044 ở đâu khi MCU reset |
| 1–3b | L1, nạp, nhịp L2 20 lần / 10 s | board sống, clock đúng |
| 4 | `0x1FFFF7CC` = `0x0040` | dung lượng flash trên chip thật (B5) |
| 5, 5b | PB6 ≈ 0 V; độ rộng xung lúc bật nguồn | **B6 #1** |
| 6 | PA10 và STB ≥ 3 V | **B6 #2** |
| 7 | Rút–cắm nguồn 3 lần, SWD vẫn kết nối | bẫy §A3 |
| 8a–8c | Bus cô lập; **analyzer phát, phải không có ACK**; bus thật không thấy `0x400`–`0x49F` | **B6 #3** |
| 9 | Cắm/rút cảm biến 3 lần | "không có gì xảy ra" là kết quả đúng |

**Một điều chỉnh so với B6 #3 của prompt:** "mainboard báo lỗi ACK" chỉ đúng khi module này là **node duy nhất khác** trên bus — có node nào khác ACK là người phát hài lòng, dù module này im lặng hay không. Bài 8b dùng analyzer làm node duy nhất còn lại, nên không có ACK ⇔ module không ACK. Vẫn giữ biến thể "chỉ mainboard + module" cho trường hợp không có analyzer (mainboard phát GPS ở `0x600`–`0x60F`).

## 7. Vấn đề còn mở

1. ~~**Chưa test trên board.**~~ Đã test 2026-10-07, đạt — mục 8.
2. **`GND12` KHÔNG nối với `GND` trên board** (netlist: net `GND12` chỉ có J1-3, JP2-7, JP2-8; sơ đồ dùng hai ký hiệu đất riêng, không có net-tie). MAX485 không cách ly, và bộ thu chỉ chịu chế độ chung **−7 V … +12 V** so với GND của nó (`MAX481.PDF` trang 2, điều kiện của VTH và RIN) ⇒ RS485 chỉ chắc chắn đúng khi âm hai nguồn nối với nhau **bên ngoài** board. Bàn test: nối âm 12 V với âm 5 V. Người dùng (2026-10-07): đây là **hệ data logger độc lập, không liên quan tới xe**. Việc còn mở: trong hệ đó, nguồn 12 V cho JP2 và nguồn 5 V của harness CAN1 có chung âm không. **Không chặn giai đoạn 2** (bàn test nối chung âm); cần biết trước khi lắp đặt thật. Nếu không chung — nối chung trong dây nguồn, hoặc sửa PCB (nối `GND12` với `GND`), hoặc đổi sang transceiver cách ly.
3. **JP2 chân 5/6 là +12 V ngay cạnh SWD.** Quy trình test đã ghi: nạp khi chưa cấp +12 V. Lần ra PCB sau nên tách nguồn 12 V khỏi header nạp.
4. **Cửa sổ MAX485 phát lúc bật nguồn** (reset → `main`) không xoá được bằng firmware. Lần ra PCB sau: R8/R9 xuống GND thay vì lên +5 V ⇒ MAX485 mặc định ở chế độ nhận.
5. **`VF_Main` không có bản nào của `VF_TH_CAN_Proposal.md`** (kiểm 2026-10-05; module góc thì có ở `VF_Main/docs/VF_Angle/`). Bản gốc thực tế là `docs/VF_TH_CAN_Proposal.md` của repo này — và nó **nằm ngoài vùng `check_contracts` canh**. Việc ở phiên `VF_Main`: thêm `docs/VF_TH/VF_TH_CAN_Proposal.md`.
6. **Hai lỗi chữ trong hợp đồng** (ghi ở `contracts/README.md`, sửa ở nhà chính thức lần bump sau; **không ảnh hưởng firmware**):
   - proposal §5.3, tiêu đề `TH_Cmd — 0x040 + node_id` → đúng là `0x010 + node_id`;
   - CAN2 Protocol v1.5 bảng §4.1, dòng `0x03` vẫn ghi *chưa đặt tên*.
7. **Shield của J1 thả nổi** (S1/S2 chỉ nối với nhau). CLAUDE.md §4.2 trước đây ghi "4 = RS485_A (+shield)" — đã sửa. Nếu dây dài có nhiễu ở giai đoạn 2, đây là nghi can thứ hai sau việc thiếu 120 Ω.
8. **Không có watchdog** (IWDG chưa bật trong `.ioc`). Giai đoạn 1 không cần; giai đoạn 2 trở đi một Modbus treo phải được bắt bằng timeout (đã có `MB_REPLY_TIMEOUT_MS`), nhưng một vòng lặp treo thì chỉ IWDG bắt được. Đề xuất bật ở giai đoạn 6 — cần sửa `.ioc` ⇒ người dùng làm.
9. **DE bằng phần mềm ở giai đoạn 2:** PB6 là push-pull, mức cao của nó là 3,3 V trong khi R8/R9 kéo lên 5 V. MAX485 nhận 3,3 V là HIGH hợp lệ (`MAX481.PDF` trang 2, DC Electrical Characteristics: VIH của DE, DI, RE̅ = **2,0 V min**, VIL = 0,8 V max); dòng chảy ngược (5 − 3,3) V / 5 kΩ ≈ 0,34 mA, vô hại. Chỉ ghi lại để giai đoạn 2 không ngạc nhiên khi đo thấy 3,3 V thay vì 5 V lúc phát.

# 8. Kết quả test trên board — 2026-10-07

Người dùng chạy `docs/VF_TH_Phase1_Test.md` trên board đầu tiên, image của commit `b67378b`.

Người dùng đánh số **lệch một** so với quy trình (bài 0 của quy trình được báo là "bài 1"…). Bảng dưới dùng **số của quy trình**, đối chiếu theo nội dung từng bài:

| # quy trình | Báo là | Bài | Kết quả |
|---|---|---|---|
| 0 | bài 1 | Giữ SW1, đo PB6 và STB | **PB6 = 5 V, STB = 3,3 V** — xem 8.1 |
| 1 | bài 2 | Cấp nguồn, chưa nạp | **PASS** — L1 sáng, L2 tối |
| 2 | bài 3 | Nạp qua SWD | **PASS** |
| 3 | bài 4 | Nhịp L2 | **PASS** — nháy ngắn 2 lần/giây |
| 3b | — | Live Expressions | không chạy (tuỳ chọn) |
| 4 | bài 5 | `0x1FFFF7CC` | **PASS** — `0x0040` = 64 KB ⇒ đúng STM32F303**K8** |
| **5** | báo riêng, 2026-10-07 | **PB6 khi firmware đang chạy** | **PASS — PB6 = 0 V** |
| 5b | — | Độ rộng xung PB6 lúc bật nguồn | không chạy (tuỳ chọn) |
| 6 | bài 6 | PA10 và STB | **PASS** — cả hai ≈ 3,3 V: TJA1044 ở Standby |
| 7 | bài 7 | Rút–cắm nguồn, SWD kết nối lại | **PASS** — `.ioc` không tắt SWJ |
| 8a | bài 8a | Bus cô lập, rút–cắm nguồn 5 lần | **PASS** — không frame, không error frame |
| 8b | bài 8b | Analyzer phát, không ai khác trên bus | **PASS** — không có ACK ⇒ **module không ACK** |
| 8c | bài 8c | Bus CAN1 thật | **PASS** — không có gì trong `0x400`–`0x49F` |
| 9 | bài 9 | Cắm/rút cảm biến 3 lần | **PASS** — không có gì xảy ra |
| — | — | Dòng tiêu thụ 5 V / 12 V | bỏ qua (người dùng — chỉ là mốc) |
| — | — | Chung âm 5 V / 12 V? | hệ data logger độc lập, không liên quan tới xe — xem việc mở 7.2 |

## 8.1 Điều đã được chứng minh

- **R8/R9 có lắp trên board thật** (bài 0: PB6 = 5 V khi MCU bị giữ reset). Bẫy CLAUDE.md §4.3 là có thật, không phải chỉ trên netlist: **từ lúc có điện tới `app_board_early_guard()`, MAX485 đang phát**.
- **Và firmware thắng được R8/R9** (bài 5: PB6 = **0 V** khi chạy) — B6 #1. Cặp số 5 V / 0 V của bài 0 và bài 5 là bằng chứng trực tiếp duy nhất: bài 9 không thay được (giai đoạn 1 không nói chuyện với cảm biến, nên MAX485 phát hay nhận thì cũng "không có gì xảy ra"), đo A/B cũng không (driver bật với DI = 1 cho cùng cực tính với phân cực fail-safe).
- **Trong lúc MCU reset, STB vẫn ở mức cao (3,3 V)** dù PA10 thả nổi — cùng mức với lúc firmware chủ động giữ Standby (bài 6). Tức là có thứ giữ STB lên khi không ai lái nó (nhiều khả năng là pull-up nội của TJA1044; datasheet không có trong repo). Hệ quả: **transceiver không vào Normal trong cửa sổ reset**. Bài 8a (rút–cắm nguồn 5 lần khi analyzer nghe, không một error frame nào) xác nhận cùng điều đó từ phía bus.
- **Module im lặng trên CAN1, cả phát lẫn ACK** (8a, 8b, 8c). 8b là phép thử thật của B6 #3: analyzer là node duy nhất còn lại và không nhận được ACK.
- **SWD sống sót qua mất nguồn** (bài 7).
- **Chip là K8, 64 KB** (bài 4) — phân vùng bootloader 12 KB + ứng dụng 52 KB của phase 5 đứng vững.
- Nhịp 2 lần/giây ⇒ SysTick đúng 1 ms ⇒ gián tiếp xác nhận PLL 72 MHz lên từ HSE 8 MHz. HSE chết thì `Error_Handler` đã nháy mã 1.

## 8.2 Tiêu chí nghiệm thu — cả 6 đạt

| # | Tiêu chí | Kết quả |
|---|---|---|
| 1 | Build 0 error, mọi warning được giải thích | 0 error, 0 warning (IDE + headless) |
| 2 | `check_contracts.sh` chứng minh fail được | mục 4: lệch 1 byte và sót DBC cũ đều dừng build |
| 3 | Báo cáo flash/RAM, dung lượng MCU đã xác nhận | mục 5; trên chip `0x1FFFF7CC` = `0x0040` |
| 4 | LED nháy, duty ≤ 10% | bài 3; duty kiểm ở thì biên dịch |
| 5 | Ba phép đo B6 đạt | PB6 = 0 V (bài 5), PA10/STB ≈ 3,3 V (bài 6), không ACK (bài 8b) |
| 6 | Quy trình test ở `docs/VF_TH_Phase1_Test.md` | có, và đã chạy |

**Giai đoạn 1 khép lại 2026-10-07.** Các việc còn mở ở mục 7 (trừ mục 1) chuyển sang CLAUDE.md §7.2 và kế hoạch giai đoạn 2.
