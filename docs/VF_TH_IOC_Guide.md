# VF_TempHumi_Module — hướng dẫn tạo `.ioc`

Người dùng làm phần này **bằng tay trong CubeMX**, trước khi phiên Claude Code bắt đầu. Claude không được sửa `.ioc` (CLAUDE.md §6.1).

Cách trình bày theo `VF_Angle_Module/docs/VF_Angle_IOC_Guide.md` — nếu một mục ở đây nói ngắn, mục tương ứng bên đó nói đủ.

---

## A1. Tạo project

| Mục | Giá trị |
|---|---|
| MCU | **STM32F303K8Tx** (LQFP32) |
| Project Name | `VF_TempHumi_Module` |
| Toolchain | STM32CubeIDE |
| Code generator | **Generate peripheral initialization as a pair of .c/.h files** ✓ |

Khi wizard hỏi "Initialize all peripherals with their default Mode?" → **No**.

> **Đối chiếu pinout ngay ở màn hình Pinout view.** `CLAUDE.md` §4.1 **suy ra** bản đồ chân từ netlist chứ không đọc datasheet (datasheet F303K8 không có trong repo). CubeMX biết package — nhìn vào nó và xác nhận pin 19 = `PA9`, pin 29 = `PB6`, pin 30 = `PB7`, pin 21/22 = `PA11`/`PA12`. Nếu lệch, **dừng lại và báo**.

---

## A2. RCC — thạch anh ngoài

`System Core → RCC`:

| Mục | Giá trị |
|---|---|
| High Speed Clock (HSE) | **Crystal/Ceramic Resonator** |
| Low Speed Clock (LSE) | Disable |

Board có Y1 **8 MHz** với C9/C13 20 pF và R6 1 M.

---

## A3. SYS — mục quan trọng nhất của cả hướng dẫn

`System Core → SYS`:

| Mục | Giá trị |
|---|---|
| **Debug** | **Serial Wire** |
| Timebase Source | SysTick |

> ⛔ **Nếu để Debug = No Debug, CubeMX sinh code tắt SWD, và board sẽ chết SWD ở mỗi lần bật nguồn.** JP2 không có NRST, nên đường cứu duy nhất là giữ SW1 trong lúc kết nối — và trên một board đã nạp image như vậy thì cả cách đó cũng khó. **Kiểm lại sau mỗi lần Generate**: `Core/Src/gpio.c` **không** được chứa `__HAL_AFIO_REMAP_SWJ_DISABLE` hay tương đương.

---

## A4. Clock Configuration — 72 / 36 / 72 MHz

```
HSE 8 MHz → PLLMUL ×9 → SYSCLK 72 MHz
  HCLK  (AHB)  72 MHz
  PCLK1 (APB1) 36 MHz   <- /2, bat buoc: APB1 toi da 36 MHz
  PCLK2 (APB2) 72 MHz
```

Cách nhanh: gõ `72` vào ô **HCLK** rồi Enter, để CubeMX tự giải. **Rồi đối chiếu bằng mắt** ba con số trên — đó là bước bắt buộc, không phải lời khuyên.

**APB1 = 36 MHz là con số mà bit timing CAN phụ thuộc vào.** Sai ở đây thì CAN sai mà không có cách nào biết cho tới lúc lên bus thật.

---

## A5. CAN — 500 kbps

`Connectivity → CAN`: **Activated** ✓

Chân: `PA11` = `CAN_RX`, `PA12` = `CAN_TX` (CubeMX tự gán).

### Parameter Settings

| Mục | Giá trị |
|---|---|
| Prescaler | **4** |
| Time Quanta in Bit Segment 1 | **15 Times** |
| Time Quanta in Bit Segment 2 | **2 Times** |
| ReSynchronization Jump Width | **1 Time** |
| Operating Mode | Normal |
| Các mục còn lại | Disable |

Kiểm lại số học, đừng tin bảng:

```
36 MHz / (4 x (1 + 15 + 2))  =  36 000 000 / 72  =  500 000 bit/s   ✓
Sample point = (1 + 15) / 18 = 88,9%
```

> Đây **đúng bằng** cấu hình của `VF_Angle_Module` (`Core/Src/can.c`: Prescaler 4, BS1 15, BS2 2, SJW 1). F103 và F303 đều chạy APB1 ở 36 MHz, nên cùng một bộ số cho cùng một tốc độ bus. Trùng khớp này không phải tình cờ và nên giữ.

### NVIC

Bật **CAN RX0 interrupt** ✓ (và `CAN SCE` nếu muốn bắt bus-off ở phase 6).

---

## A6. USART1 — 4800 8N1 tới MAX485

`Connectivity → USART1`: Mode **Asynchronous**.

> ⚠️ **Kiểm chân ngay.** CubeMX mặc định hay chọn `PA9`/`PA10` cho USART1. Board này dùng **`PA9` (TX) và `PB7` (RX)** — `PA10` đang là `EN_CAN1`. Bấm vào `PA10` trong Pinout view, đảm bảo nó **không** phải `USART1_RX`; rồi bấm `PB7` và chọn `USART1_RX`.

### Parameter Settings

| Mục | Giá trị | Nguồn |
|---|---|---|
| Baud Rate | **4800** Bits/s | datasheet cảm biến: mặc định xuất xưởng |
| Word Length | 8 Bits | |
| Parity | **None** | |
| Stop Bits | **1** | |
| Data Direction | Receive and Transmit | |
| Over Sampling | 16 Samples | |

### NVIC

Bật **USART1 global interrupt** ✓.

### Vì sao KHÔNG dùng Hardware Driver Enable của F303

USART của F3 có sẵn tính năng DE (chân RS485 Driver Enable, `USART_CR3_DEM`) với thời gian khẳng định/nhả lập trình được — nó sinh ra đúng cho việc này.

**Không dùng được ở đây:** `USART1_DE` nằm trên **`PA12`**, mà `PA12` là `CAN_TX`. Board nối DE vào `PB6`, là GPIO thường.

⇒ **Phải điều khiển DE bằng phần mềm**, và điều đó kéo theo một cái bẫy — xem §A8.

---

## A7. GPIO — ba chân, và trạng thái khởi tạo quyết định tất cả

`System Core → GPIO`. Gán ba chân thành `GPIO_Output`, rồi đặt thuộc tính:

| Chân | User Label | Output level **ban đầu** | Mode | Pull | Speed |
|---|---|---|---|---|---|
| `PA10` | `EN_CAN1` | **High** | Push Pull | No pull | Low |
| `PB6` | `ENA_RS485` | **Low** | Push Pull | No pull | Low |
| `PB0` | `LED_DEBUG` | **High** | Push Pull | No pull | Low |

Ba trạng thái ban đầu này **đều quan trọng và đều ngược trực giác**:

### `PA10` = High ⇒ transceiver CAN ở **Standby**

TJA1044: STB **LOW = Normal**, HIGH = Standby.

Module phải **im lặng trên CAN1** cho tới khi firmware đọc được một config hợp lệ (magic + CRC). Bật nguồn vào Normal nghĩa là một module chưa cấu hình sẽ ACK và có thể phát — và trùng `node_id` làm **sập bus**, không chỉ làm rối log.

> `PA10` chỉ được xuống LOW ở phase 4, và chỉ khi config có CRC đúng. Đây đúng là trình tự `VF_Angle_Module` đã làm (GĐ 6a-1).

### `PB6` = Low ⇒ MAX485 ở chế độ **nhận**

Xem `CLAUDE.md` §4.3: board kéo DE/RE lên +5 V qua 5 kΩ, nên **từ lúc có điện tới lúc GPIO được cấu hình, MAX485 đang PHÁT và giữ bus RS485**.

Đặt `Low` ở đây làm cửa sổ đó ngắn nhất có thể — nhưng **không xoá được nó**. Phase 1 phải đo bằng đồng hồ.

### `PB0` = High ⇒ LED **tắt**

L2 có anode ở +3V3, cathode qua R5 vào `PB0`. **Kéo xuống là sáng.**

---

## A8. 🪤 Cái bẫy của DE phần mềm — đọc trước khi viết driver

Đây không phải cấu hình `.ioc`, nhưng nó là hệ quả trực tiếp của §A6 và là chỗ **mọi** lần làm RS485 bằng phần mềm đều sai lần đầu.

```c
HAL_GPIO_WritePin(ENA_RS485_GPIO_Port, ENA_RS485_Pin, GPIO_PIN_SET);   /* phat */
HAL_UART_Transmit(&huart1, buf, n, 100);
HAL_GPIO_WritePin(ENA_RS485_GPIO_Port, ENA_RS485_Pin, GPIO_PIN_RESET); /* nhan */
```

**Đoạn trên cắt mất byte cuối.** `HAL_UART_Transmit()` trả về khi byte cuối đã vào thanh ghi dịch, **chưa ra khỏi dây**. Hạ DE lúc đó là tắt driver giữa lúc bit cuối đang được truyền.

Phải chờ cờ **TC** (Transmission Complete), không phải TXE:

```c
while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) { /* có timeout */ }
```

Ở 4800 baud một ký tự là **2,08 ms** — cửa sổ sai đủ rộng để nhìn thấy trên oscilloscope và đủ im lặng để không bao giờ bị nghi là nguyên nhân.

Và sau khi hạ DE, Modbus RTU đòi **im lặng 3,5 ký tự** trước khung tiếp theo: ở 4800 baud là **7,3 ms**.

---

## A9. Phân vùng flash cho bootloader

Chưa làm ở phase 1, nhưng quyết định ngay để không phải dời về sau. Theo `VF_Angle_Module`:

```
0x0800_0000  +------------------+
             | Bootloader 12 KB |   giao thuc CAN2
0x0800_3000  +------------------+
             | Ung dung         |   .isr_vector @ 0x08003000
             +------------------+
```

STM32F303K8T6 có **64 KB flash**. Module góc dùng hết 44,6 KB cho ứng dụng; module này không có hợp nhất góc, không có FIFO, không có SPI — nên còn rất rộng.

**Xác nhận dung lượng flash và RAM trong CubeMX ở phase 1** rồi ghi vào `app_config.h` làm comment. Đừng tin con số trong tài liệu này.

---

## A10. Project Manager

| Tab | Mục | Giá trị |
|---|---|---|
| Project | Toolchain | STM32CubeIDE |
| Code Generator | Generate peripheral init as pair of .c/.h | ✓ |
| Code Generator | Keep User Code when re-generating | ✓ |
| Code Generator | Delete previously generated files when not re-generated | ✓ |

---

## A11. Checklist sau khi Generate — bắt buộc

| # | Kiểm | Cách |
|---|---|---|
| 1 | SWD còn sống | `grep -r SWJ_DISABLE Core/Src/` → **không có kết quả** |
| 2 | Clock đúng | `SystemClock_Config()`: `PLLMUL = RCC_PLL_MUL9`, `APB1CLKDivider = RCC_HCLK_DIV2` |
| 3 | CAN đúng | `can.c`: Prescaler `4`, BS1 `15`, BS2 `2`, SJW `1` |
| 4 | UART đúng chân | `usart.c` `HAL_UART_MspInit`: `GPIO_PIN_9` trên `GPIOA`, `GPIO_PIN_7` trên `GPIOB` |
| 5 | Ba GPIO đúng mức ban đầu | `gpio.c`: `PA10` SET, `PB6` RESET, `PB0` SET |
| 6 | NVIC | `CAN_RX0_IRQn` và `USART1_IRQn` được bật |

Mục 5 là mục dễ mất nhất khi Generate lại, và cũng là mục **im lặng nhất khi sai**: `PB6` sai mức làm module giữ bus RS485, `PA10` sai mức làm module ACK trên bus đo trước khi được cấu hình.

## A12. *(GĐ 4)* Linker script của ứng dụng — sửa tay sau mỗi lần Generate

`STM32F303K8TX_FLASH.ld`: `FLASH ... LENGTH = 50K` (`ORIGIN` = `0x8000000` tới GĐ 5, `0x8003000` từ GĐ 5). Một lần Generate có thể trả nó về `64K`; `tools/check_linker.sh` làm build **đỏ** khi điều đó xảy ra — không phải sửa gì khác ngoài con số.

---
---

# Phần B — Project bootloader `VF_TempHumi_Bootloader` (giai đoạn 5)

Người dùng làm phần này **bằng tay trong CubeIDE / CubeMX**, trước khi Claude viết code bootloader. Claude không sửa `.ioc` (CLAUDE.md §6.1).

Bootloader **không đọc cảm biến** và **không nói Modbus**: nó chỉ có CAN, ba chân GPIO, flash. Nhưng nó **chạy trước ứng dụng sau mỗi lần cấp nguồn** — nên nó thừa hưởng nguyên cái bẫy lớn nhất của board này (CLAUDE.md §4.3): **PB6 phải xuống thấp trước mọi thứ**, vì từ lúc có điện MAX485 đang phát. Bootloader nghe ít nhất 1 s, và **ở lại mãi mãi** khi chưa có config — một bootloader quên PB6 sẽ giữ bus RS485 **vĩnh viễn**.

Cách trình bày theo `VF_Angle_Module/docs/VF_Angle_IOC_Guide.md` Phần B — mục nào ở đây nói ngắn thì mục tương ứng bên đó nói đủ.

---

## B1. Tạo project — **bên trong thư mục repo**

`File → New → STM32 Project`:

| Mục | Giá trị |
|---|---|
| MCU | **STM32F303K8Tx** (LQFP32) — giống A1 |
| Project Name | **`VF_TempHumi_Bootloader`** |
| **Location** | **bỏ chọn "Use default location"**, chọn **`<workspace>/VF_TempHumi_Module/VF_TempHumi_Bootloader`** |
| Code generator | **Generate peripheral initialization as a pair of .c/.h files** ✓ |

Khi hỏi "Initialize all peripherals with their default Mode?" → **No**.

> **Vì sao bên trong repo:** bootloader dùng chung ba file nguồn với ứng dụng (B6) qua đường dẫn tương đối, và nó phải nằm trong git **cùng commit** với ứng dụng — hai image cùng một định dạng config. Đúng như `VF_Angle_Module/VF_Angle_Bootloader/`.

---

## B2. Chỉ bật những thứ này

| Khối | Cấu hình | Giống phần A? |
|---|---|---|
| RCC | HSE = **Crystal/Ceramic Resonator** | ✅ A2 |
| Clock | **72 / 36 / 72 MHz** (HSE 8 MHz → PLL ×9; APB1 /2) | ✅ A4 — **phải giống hệt**: cùng bit timing CAN |
| **SYS** | **Debug = `Serial Wire`**, Timebase SysTick | ✅ A3 — ⛔ **mục quan trọng nhất**: bootloader chạy trước ứng dụng ở mọi lần cấp nguồn; nó tắt SWD thì board chết SWD **dù ứng dụng đúng** |
| CAN | Activated; Prescaler **4**, BS1 **15 Times**, BS2 **2 Times**, SJW **1 Time**, Normal | ✅ A5 |
| GPIO | ba chân, **cùng nhãn, cùng mức ban đầu như phần A** — bảng dưới | ✅ A7 |
| Project Manager | **Heap `0x0`**, Stack `0x400` | khác A: không `malloc` trong bootloader — heap 0 biến điều đó thành ràng buộc của linker |

### GPIO — ba chân, **nhãn phải giống hệt phần A**

| Chân | User Label | Output level **ban đầu** | Mode | Pull | Speed |
|---|---|---|---|---|---|
| `PA10` | **`EN_CAN1`** | **High** (standby) | Push Pull | No pull | Low |
| `PB6` | **`ENA_RS485`** | **Low** (MAX485 nhận) | Push Pull | No pull | Low |
| `PB0` | **`LED_DEBUG`** | **High** (LED tắt) | Push Pull | No pull | Low |

> ⚠️ **Nhãn không phải chuyện thẩm mỹ.** Bootloader link **`app_board.c` của ứng dụng** (B6) để dùng **đúng** hàm `app_board_early_guard()` đã đo trên board ở GĐ 1 (PB6 = 5 V khi reset, 0 V khi chạy). `app_board.c` dùng `ENA_RS485_Pin`, `EN_CAN1_Pin`, `LED_DEBUG_Pin` từ `main.h`. Sai một chữ ⇒ không build được — đó là điều tốt. **Nhãn đúng mà chân sai** thì không gì bắt được, nên đối chiếu chân với bảng §4.1 CLAUDE.md.

### KHÔNG bật

- **USART1** (PA9 / PB7): bootloader không nói chuyện với cảm biến. Để PA9, PB7 **chưa gán**.
- Mọi ngắt CAN trong NVIC (`CAN RX0`, `CAN SCE`…): bootloader **thăm dò** FIFO. Mỗi ngắt bật lên là thêm một thứ phải gỡ trước khi nhảy sang ứng dụng.

NVIC chỉ còn `Time base: System tick timer`.

---

## B3. Generate Code, rồi tự kiểm

| # | Kiểm | Cách | Phải thấy |
|---|---|---|---|
| 1 | **SWD còn sống** | `grep -r SWJ_DISABLE VF_TempHumi_Bootloader/Core/Src/` | **không có kết quả** |
| 2 | Clock | `SystemClock_Config()` trong `main.c` | `RCC_PLL_MUL9`, `RCC_HCLK_DIV2` — **giống từng dòng** với `Core/Src/main.c` của ứng dụng |
| 3 | CAN | `can.c` | Prescaler `4`, BS1 `15`, BS2 `2`, SJW `1` |
| 4 | **Ba GPIO** | `gpio.c` | `LED_DEBUG` **SET**, `EN_CAN1` **SET**, `ENA_RS485` **RESET**; ba nhãn trong `main.h` đúng chữ |
| 5 | Không USART | `Core/Src/` | **không có** `usart.c` |
| 6 | Không ngắt CAN | `HAL_CAN_MspInit` trong `can.c` | **không có** `HAL_NVIC_EnableIRQ` |

---

## B4. Linker script của bootloader — sửa tay, sau Generate

Mở `VF_TempHumi_Bootloader/STM32F303K8TX_FLASH.ld`:

```
FLASH (rx) : ORIGIN = 0x8000000, LENGTH = 12K
```

Bản đồ flash chốt ở GĐ 4 (`app_config.h`):

```
0x08000000  bootloader   12 KB   <- project nay
0x08003000  ung dung     50 KB
0x0800F800  config        2 KB   (trang cuoi; trang F3 = 2 KB)
```

> ⚠️ **`ORIGIN` của ỨNG DỤNG chưa đổi ở bước này.** Ứng dụng chỉ dời sang `0x8003000` **cùng lúc** với lần nạp bootloader đầu tiên — quy trình test GĐ 5 sẽ ghi rõ bước đó. Dời sớm khi chip chưa có bootloader là board **không khởi động được**: lúc reset CPU đọc con trỏ stack và vector reset ở `0x08000000`, chỗ đó trắng (`0xFFFFFFFF`), và nó hard-fault ngay — không LED, không gì cả, giống hệt một board chết.

`tools/check_linker.sh` (từ GĐ 4) sẽ được mở rộng để kiểm **cả hai** linker script: bootloader kết thúc ≤ `0x08003000`; ứng dụng bắt đầu ở `0x08003000` và kết thúc ≤ `0x0800F800`.

---

## B5. Mức tối ưu: `-Og`

`Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Optimization → Optimization level = Optimize for debug (-Og)` — cho cấu hình **Debug**.

Số đo của module rung (F103, cùng chức năng): `-O0` = 13 100 B — **tràn 12 KB**; `-Og` = 8 680 B. HAL của F303 lớn hơn F103 (riêng `HAL_RCC_OscConfig` đã 4 KB ở `-O0` trong ứng dụng này), nên `-O0` gần như chắc chắn tràn. Kích thước thật báo sau lần build đầu; nếu `-Og` vẫn sát trần, dự phòng là `-Os`.

---

## B6. Dùng chung code với ứng dụng — **link từng file, không chép**

Ba file của ứng dụng được **link** vào bootloader:

| File (`VF_TempHumi_Module/Core/Src/app/`) | Vì sao |
|---|---|
| `app_cfg.c` | định dạng config + phép kiểm. CAN2 §5 bất biến 5: bootloader dùng **đúng hàm** ứng dụng dùng |
| `app_cfg_store.c` | đọc / ghi / xoá page config — `COMMIT` ghi bằng **đúng đường** ứng dụng đọc |
| `app_board.c` | **`app_board_early_guard()`** — PB6 xuống thấp trước `HAL_Init`, đúng code đã đo ở GĐ 1 |

**Cách link một file:** chuột phải `Core/Src` của **project bootloader** → `New → Folder`, tên `app` (nếu chưa có). Rồi chuột phải `Core/Src/app` → `New → File` → **`Advanced >>`** → ✓ **`Link to file in the file system`** → `Browse…` tới file trong `VF_TempHumi_Module/Core/Src/app/` → Finish. Làm ba lần.

> **Đừng link cả thư mục.** Link từng file là danh sách trắng: đúng ba file sang. Link thư mục là danh sách đen — một file mới thêm bên ứng dụng sẽ tự lọt sang và gây `multiple definition`, hoặc kéo theo cả HAL UART.
>
> Ở module rung, bước này từng hỏng vì file được tạo **trên nhầm project**. Kiểm: trong Project Explorer ba file có **mũi tên nhỏ** trên icon, và nằm dưới `VF_TempHumi_Bootloader/Core/Src/app/`.

**Include path** — `Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Include paths`, chọn **Configuration: [All configurations]**, thêm:

```
"${workspace_loc:/VF_TempHumi_Module/Core/Inc}"
```

Để nó **sau** `../Core/Inc`: `main.h` của **bootloader** phải được tìm thấy trước `main.h` của ứng dụng. Các header `app/app_*.h` chỉ có bên ứng dụng nên được tìm thấy ở đường thứ hai.

> **Đã tách xong (2026-10-07):** `app_cfg_store.c` giờ chỉ đọc/ghi/xoá flash — ký hiệu ngoài duy nhất nó cần là `app_cfg_seal` (trong `app_cfg.c`), HAL flash và `memcpy`/`memcmp`/`memset`. Phần `g_diag` / LED / công tắc bench đã sang `app_cfg_boot.c`, **chỉ của ứng dụng — đừng link file này**.
>
> Project **ứng dụng** có một file mới (`app_cfg_boot.c`): **Refresh + Build Project** ứng dụng một lần.

---

## B7. `makefile.init` của bootloader

**Claude viết** `VF_TempHumi_Bootloader/makefile.init`: chạy `check_contracts.sh` và `check_linker.sh` của repo trước mỗi lần build bootloader. Bạn không làm gì ở bước này.

---

## B8. Build một lần, rồi báo cho Claude

1. Chuột phải `VF_TempHumi_Bootloader` → **Refresh** → **Build Project**. Lỗi thiếu `boot_*` hay `g_diag` ở lần đầu là bình thường — code bootloader chưa được viết.
2. Báo cho Claude: B3 đạt cả 6 mục; linker bootloader đã `LENGTH = 12K`; `-Og` đã đặt; ba file đã link; include path đã thêm cho **All configurations**.

Claude kiểm lại tất cả từ file trên đĩa trước khi viết code.
