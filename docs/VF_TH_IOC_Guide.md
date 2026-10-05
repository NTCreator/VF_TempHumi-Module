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
