# CLAUDE.md — VF_TempHumi_Module

File này là bối cảnh cố định của dự án. Đọc hết trước mỗi phiên và tuân thủ tuyệt đối mục "Quy tắc bắt buộc".

## 1. Sản phẩm

Module đo **nhiệt độ và độ ẩm**, cắm lên bus CAN1 của `VF_Main` cùng với module rung và module góc. MCU **STM32F303K8T6**, cảm biến **SHT RS485 Modbus-RTU** nối qua **MAX485**.

Module này là **loại `0x3`** trong quy tắc nibble — loại thứ ba được định nghĩa, và dải ID của nó đã được dành sẵn từ v1.0 của hợp đồng.

### 1.1 Điều quan trọng nhất về sản phẩm này

**Cảm biến chỉ làm mới giá trị mỗi 2 giây** (datasheet, mục "Thời gian làm mới"). Thời gian đáp ứng còn chậm hơn: ≤25 s cho nhiệt độ, ≤8 s cho độ ẩm ở gió 1 m/s.

Hệ quả chi phối toàn bộ thiết kế:

- Hỏi cảm biến nhanh hơn 2 s **không cho số mới**, chỉ tốn bus RS485.
- Một giá trị cũ **trông y hệt** một giá trị mới. Đây đúng là lỗi đã xảy ra với `VIB_Status.Temperature` của module rung: nó đóng băng giá trị lúc khởi động và phát lại mỗi giây mãi mãi, trông như một cảm biến tốt trong phòng ổn nhiệt, và **không ai phát hiện ra trong nhiều tuần** (xem `VF_Main/docs/VF_Main_Remaining_Work.md` §4.3).

> **Vì vậy `TH_Data` BẮT BUỘC mang theo tuổi của số đo.** Không có trường đó thì dữ liệu sai không thể phát hiện được từ bên trong file log — và đó là loại hỏng tệ nhất mà dự án này đã gặp.

### 1.2 Đây là HAI firmware, không phải một

Giống module rung và module góc:

| | |
|---|---|
| **Bootloader** | chứa giao thức cấu hình CAN2. Module có ứng dụng hỏng vẫn phải cấu hình được — đó chính là lúc cần nhất |
| **Ứng dụng** | đo và phát lên CAN1 |

**Config không hợp lệ (magic/CRC sai) thì module IM LẶNG trên CAN1**, chỉ trả lời trên CAN2. Không bao giờ đoán một `node_id` rồi phát: hai module cùng ID phát cùng lúc gây bit error, error frame, và có thể đẩy cả hai vào bus-off — trùng ID làm **sập bus** chứ không chỉ làm rối log.

## 2. Tài liệu tham chiếu (nguồn sự thật)

### 2.1 File hợp đồng — CHỈ ĐỌC

Nhà chính thức của ba file này là repo **`VF_Main`**. Ở đây chúng là bản sao trong `contracts/`, và `tools/check_contracts.sh` so hash trong mỗi lần build — lệch là **build fail và gọi tên file**.

```
contracts/VF_E-Drive_Data_Logger_v1.<x>.dbc
contracts/VF_Main_CAN_ID_Map.md
contracts/VF_Main_CAN2_Service_Protocol.md
```

Script so cả **tập tên file**, nên khi DBC lên version mới thì bản cũ phải bỏ đi — một bản cũ sót lại cũng làm build fail, và đó là hành vi đúng.

**Không bao giờ sửa file trong `contracts/`.** Cần đổi thì sửa ở `VF_Main`, bump version, rồi đồng bộ bản sao.

### 2.2 Tài liệu phần cứng — trong `docs/`

| File | Nội dung |
|---|---|
| `D__..._VF_TempHumi_SCH.NET` | Netlist. **Nguồn sự thật cho mọi câu hỏi về chân** |
| `VF_TempHumi Module.pdf` | Sơ đồ nguyên lý |
| `[Hshop.vn] SHT RS485 Modbus-RTU...pdf` | Datasheet cảm biến: bảng thanh ghi, khung Modbus, ví dụ có số thật |
| `MAX481.PDF` | Datasheet họ MAX481/483/485/487–489 |

**Datasheet STM32F303K8T6 KHÔNG có trong repo.** Cần số liệu điện hay pinout thì hỏi người dùng, hoặc đối chiếu trong CubeMX (nó biết package). **Không đoán.**

Khi viết một hằng số lấy từ datasheet, **ghi nguồn trong comment** (tên tài liệu và mục).

## 3. Môi trường

- STM32CubeIDE, HAL (không dùng LL trừ khi được yêu cầu).
- **Không có RTOS.** Vòng lặp chính + ngắt, như hai module anh em.
- **Không có hardware trong vòng lặp:** không flash, không debug được. Mọi bước kiểm trên phần cứng phải viết thành quy trình test để người dùng tự làm.
- Build headless hoặc nhờ người dùng build rồi dán log. **Không coi một phase là xong khi chưa build sạch** (0 error; mọi warning phải được giải thích).

## 4. Phần cứng

### 4.1 Bản đồ chân — suy từ `VF_TempHumi_SCH.NET`

Suy ra từ netlist bằng cách neo vào bảy điểm chắc chắn (VDD pin 1/5/17, VSS pin 16/32, NRST pin 4, SWD pin 23/24, OSC pin 2/3). **Đối chiếu lại trong CubeMX ở phase 1** trước khi tin.

| Chức năng | Chân MCU | Pin | Ghi chú |
|---|---|---|---|
| CAN_RX | **PA11** | 21 | → TJA1044 chân 4 |
| CAN_TX | **PA12** | 22 | → TJA1044 chân 1 |
| EN_CAN1 (STB) | **PA10** | 20 | qua R2 10k → TJA1044 chân 8. **LOW = Normal** |
| USART1_TX → MAX485 DI | **PA9** | 19 | R7 10k kéo lên +5 |
| USART1_RX ← MAX485 RO | **PB7** | 30 | R10 10k kéo lên +5. PB7 là chân FT |
| **ENA_RS485 (DE + RE̅)** | **PB6** | 29 | xem §4.3 — **có bẫy** |
| LED_DEBUG | **PB0** | 14 | **tích cực LOW** (anode ở +3V3) |
| SWDIO / SWCLK | PA13 / PA14 | 23 / 24 | JP2 |
| BOOT0 | PB8 | 31 | R1 10k kéo xuống |
| NRST | — | 4 | **SW1 nút reset trên board** — giao thức CAN2 cần nó |
| OSC_IN / OSC_OUT | PF0 / PF1 | 2 / 3 | thạch anh **8 MHz** |

**USART1_RX nằm trên PB7, KHÔNG phải PA10.** PA10 đang là EN_CAN1. Đây là chỗ dễ sai nhất khi cấu hình `.ioc`.

### 4.2 Hai connector, hai nguồn

| | Chân | Nội dung |
|---|---|---|
| **JP3** | 1,2 = +5 · 3,4 = CAN_H · 5,6 = CAN_L · 7,8 = GND | bus CAN1 |
| **JP2** | 1 = GND · 2 = SWCLK · 3 = SWDIO · 4 = +3V3 · 5,6 = **+12** · 7,8 = GND12 | nạp + nguồn 12V |
| **J1** | 1 = +12 · 2 = RS485_B · 3 = GND12 · 4 = RS485_A (+shield) | cảm biến |

**Module cần HAI nguồn:** +5 V từ harness CAN1 (qua D1 → LDO RT9193 → 3V3), và **+12 V riêng qua JP2** cho cảm biến. Cảm biến chịu 5–28 VDC nhưng board đưa 12 V tới J1.

**JP2 không có NRST** — SWD là đường debug duy nhất qua header, giống `VF_Main`. Nhưng board **có SW1** nên vẫn reset tay được.

### 4.3 ⛔ Bẫy lớn nhất của board này: DE/RE kéo lên +5 V

```
NODENAME ENA_RS485 $   R8 1   R9 1   U1 29   U4 2   U4 3
R8 pin2 → +5        R9 pin2 → +5
```

**Hai điện trở 10k song song (5k) kéo DE/RE lên +5 V.** Nghĩa là:

> Từ lúc có điện tới lúc firmware cấu hình PB6 thành output và kéo xuống, **MAX485 đang ở chế độ PHÁT và giữ bus RS485**.

Ba hệ quả bắt buộc:

1. **PB6 phải được đặt LOW sớm nhất có thể** trong `main()`, trước mọi thứ khác.
2. Trong `.ioc`, PB6 phải để `GPIO_PIN_RESET` làm trạng thái khởi tạo — xem `docs/VF_TH_IOC_Guide.md`.
3. **Đo bằng đồng hồ ở phase 1**: PB6 so GND phải ~0 V sau khi firmware chạy. Nếu board thật khác netlist (R8/R9 có thể là DNP), ghi lại sự thật đo được chứ đừng tin netlist.

PB6 là chân FT nên chịu được 5 V, và kéo 5k xuống đất chỉ tốn ~1 mA — GPIO thừa sức.

### 4.4 Những sai lệch phần cứng khác phải biết

| | |
|---|---|
| **Không có điện trở đầu cuối 120 Ω** cho RS485 trên board | Dây tới cảm biến ngắn thì thường không cần. Nếu lỗi CRC xuất hiện ở dây dài, đó là nghi can đầu tiên |
| R11 10k kéo RS485_A lên +5, R12 10k kéo RS485_B xuống GND | Phân cực fail-safe: bus rảnh đọc ra mức idle thay vì nhiễu |
| L1 là **LED nguồn**, nối cứng +3V3 → R3 → GND | Không điều khiển được, luôn sáng khi có điện |
| LED_DEBUG (L2) tích cực **LOW** | `HAL_GPIO_WritePin(..., GPIO_PIN_RESET)` là **sáng** |

## 5. Ngân sách bus

Quy tắc ở `contracts/VF_Main_CAN_ID_Map.md` §2.2: **tổng frame-slot trên toàn bus ≤ 20**, trong đó

```
1 frame-slot = 1 frame du lieu moi 10 ms = 2.22% tai bus
```

Module này phát **1 frame mỗi 1000 ms**, nên nó tốn:

```
1 / 100 = 0.01 frame-slot
```

**Mười module loại 0x3 cộng lại là 0.1 slot.** Trên thực tế loại module này **không tốn gì** của ngân sách bus. Đó là hệ quả trực tiếp của việc cảm biến chỉ làm mới mỗi 2 s — nhanh hơn là vô nghĩa.

ID Map §2.2 cảnh báo rõ: *"phải tính, đừng giả định nó giống module rung"*. Đã tính, và kết quả là **nhẹ hơn khoảng 100 lần**.

## 6. Quy tắc bắt buộc

### 6.1 Code do CubeMX sinh ra
- **Chỉ** viết code trong các cặp `/* USER CODE BEGIN … */` / `/* USER CODE END … */`.
- Không sửa `.ioc`, `Drivers/`. Nếu một thay đổi bắt buộc nằm ngoài USER CODE: **dừng lại**, giải thích, đề xuất, chờ duyệt.

### 6.2 Tổ chức file
- Code ứng dụng trong `Core/Src/app/` và `Core/Inc/app/`, include dạng `#include "app/xxx.h"`.
- Driver ngoại vi tiền tố `drv_`, module ứng dụng tiền tố `app_`. Biến nội bộ `static`.
- Mọi hằng số tập trung ở `Core/Inc/app/app_config.h`.

### 6.3 Không có RTOS
Vòng lặp chính + ngắt. **Không busy-wait mà thiếu timeout.** Modbus là giao thức hỏi–đáp có thể không bao giờ được trả lời: mọi lần chờ phải có hạn và mọi lần hết hạn phải **tăng một bộ đếm**.

> Một `return` sớm trong đường dữ liệu mà không tăng bộ đếm là một cách thất bại vô hình. Bài học này đã tốn `VF_Main` hai lượt nạp firmware.

### 6.4 Dấu chấm động trên Cortex-M4
STM32F303K8T6 **có FPU đơn chính xác**. Dùng `float` thoải mái, **không dùng `double`** (sẽ bị mô phỏng bằng phần mềm).

### 6.5 Phần cứng: những việc không được làm
- Không cấu hình lại PA13/PA14 (SWD là đường debug duy nhất qua header).
- Không để PB6 ở trạng thái input hay high khi không chủ động phát — nó giữ bus RS485.
- Không để LED_DEBUG sáng liên tục; nháy duty thấp.

### 6.6 Phong cách code
- C11, kiểu có kích thước cố định (`uint16_t`…). **Comment trong code viết bằng tiếng Anh.**
- Kiểm tra mọi `HAL_StatusTypeDef`. Lỗi đưa về một module báo lỗi chung, không âm thầm bỏ qua.
- Không magic number giữa code.

### 6.7 Quy trình làm việc
1. Mỗi phase bắt đầu bằng **kế hoạch**: file sẽ tạo/sửa, API dự kiến, rủi ro. **Chờ duyệt rồi mới code.**
2. Code xong phải build sạch.
3. Báo cáo cuối phase: tóm tắt · file đã đổi · kết quả build và flash/RAM · **quy trình test trên phần cứng** · vấn đề còn mở.
4. Mỗi phase một commit git, dạng `phaseN: <mô tả>`.
5. Chỗ nào chưa rõ thì **hỏi**, đừng tự chọn một phương án lớn.

## 7. Trạng thái

| GĐ | Nội dung | Trạng thái |
|---|---|---|
| **0** | **Hợp đồng**: proposal, DBC v1.5, ID Map v1.3, CAN2 v1.5 | **chưa duyệt** — xem `docs/VF_TH_CAN_Proposal.md` |
| 1 | Khung project, `app_config.h`, LED, im lặng trên CAN, `check_contracts` | chưa làm |
| 2 | Driver RS485 + Modbus RTU, đọc được nhiệt độ/độ ẩm | chưa làm |
| 3 | Phát `TH_Data` + `TH_Status` lên CAN1 (silent loopback trước) | chưa làm |
| 4 | Kho config trong flash + phát thật trên bus | chưa làm |
| 5 | Bootloader + giao thức CAN2 | chưa làm |
| 6 | Cờ lỗi, self-test, tích hợp với `VF_Main` | chưa làm |

**Giai đoạn 0 chặn tất cả.** ID và layout signal ràng buộc firmware của cả 10 module; đổi về sau nghĩa là nạp lại tất cả.
