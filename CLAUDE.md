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
- Build headless (đã dùng ở phase 1):
  ```
  P="/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins"
  T="$P/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
  M="$P/com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.200.202604021615/tools/bin"
  export PATH="$T:$M:$PATH"
  cd Debug && make all
  ```
  Đường dẫn Windows `C:/...` làm hỏng PATH (bị tách ở dấu `:`). Chỉ dùng dạng MSYS.
- **`Debug/makefile` do CubeIDE sinh và chỉ biết những file `.c` có lúc IDE build lần cuối.** Thêm file nguồn hay thư mục mới ⇒ người dùng làm **Refresh (F5) rồi Project → Build Project** một lần trong IDE; sau đó `make` headless mới thấy file đó. IDE thường đang mở nên workspace bị khoá — không chạy `headless-build` được. `Debug/` nằm trong `.gitignore`.
- **`check_contracts` chạy ở đầu mọi lần `make`** qua `makefile.init` ở gốc repo (hook CDT: `Debug/makefile` mở đầu bằng `-include ../makefile.init`). `make clean` không bị chặn. `.gitattributes` ghim LF cho `contracts/*` — thiếu nó thì một clone mới lệch cả bốn hash.
- **Test host** (từ phase 2): `gcc` 6.3 của MinGW có trong PATH (`/c/MinGW/bin`), `make` thì không — dùng `make` của CubeIDE (`M=` ở trên): `cd tests && PATH="/c/MinGW/bin:$M:$PATH" make`. Cờ `-Wall -Wextra -Werror -Wconversion -Wdouble-promotion`. Module thuần (`drv_modbus`, `app_meas`) không include HAL để test được ở đây.
- Toolchain ST 14.3 bỏ qua `__attribute__((retain))`. Muốn giữ một hằng không ai tham chiếu qua `--gc-sections`, cho nó một tham chiếu thật — xem `s_fw_id` trong `main.c`.

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
| **J1** | 1 = +12 · 2 = RS485_B · 3 = GND12 · 4 = RS485_A | cảm biến. Shield (S1/S2) chỉ nối với nhau — **thả nổi** |

**Module cần HAI nguồn:** +5 V từ harness CAN1 (qua D1 → LDO RT9193 → 3V3), và **+12 V riêng qua JP2** cho cảm biến. Cảm biến chịu 5–28 VDC nhưng board đưa 12 V tới J1.

**`GND12` KHÔNG nối với `GND` trên board** (netlist: net `GND12` chỉ có J1-3, JP2-7, JP2-8; sơ đồ dùng hai ký hiệu đất riêng; không có net-tie). MAX485 không cách ly, nên RS485 chỉ có chế độ chung đúng khi **âm hai nguồn được nối với nhau bên ngoài board**. Ở bàn test: nối âm 12 V với âm 5 V. Hệ thống là **data logger độc lập, không liên quan tới xe** (người dùng, 2026-10-07). Việc còn mở: trong hệ data logger, nguồn 12 V cho JP2 và nguồn 5 V của harness CAN1 có chung âm không — **cần biết trước khi lắp đặt thật, không chặn phase 2** (phase 2 chạy trên bàn test, âm đã nối chung).

**JP2 không có NRST** — SWD là đường debug duy nhất qua header, giống `VF_Main`. Nhưng board **có SW1** nên vẫn reset tay được.

**JP2 chân 5/6 là +12 V, ngay cạnh chân SWD.** Nạp khi chưa cấp +12 V: một đầu ST-Link cắm lệch hai chân là đưa 12 V vào SWDIO hoặc vào ST-Link.

### 4.3 ⛔ Bẫy lớn nhất của board này: DE/RE kéo lên +5 V

```
NODENAME ENA_RS485 $   R8 1   R9 1   U1 29   U4 2   U4 3
R8 pin2 → +5        R9 pin2 → +5
```

**Hai điện trở 10k song song (5k) kéo DE/RE lên +5 V.** Nghĩa là:

> Từ lúc có điện tới lúc firmware cấu hình PB6 thành output và kéo xuống, **MAX485 đang ở chế độ PHÁT và giữ bus RS485**.

Ba hệ quả bắt buộc:

1. **PB6 phải được đặt LOW sớm nhất có thể** trong `main()`, trước mọi thứ khác. Phase 1 làm việc này ở `USER CODE 1`, **trước `HAL_Init()`** (`app_board_early_guard()`): `SystemClock_Config()` có thể chờ thạch anh tới 100 ms, và thạch anh chết thì nhảy thẳng vào `Error_Handler` trước khi `MX_GPIO_Init()` kịp chạy. Phần còn lại của cửa sổ (reset → startup → `main`) firmware không xoá được; chỉ đổi R8/R9 xuống GND ở lần ra PCB sau mới xoá được.
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
- Không để PB6 ở trạng thái input hay high khi không chủ động phát — nó giữ bus RS485. **Áp dụng cho CẢ HAI image**: bootloader chạy trước ứng dụng ở mọi lần cấp nguồn, và **không bao giờ gọi `HAL_DeInit()`** (trên F3 nó reset bus AHB, nơi có các cổng GPIO ⇒ PB6 thả nổi ⇒ MAX485 phát).
- Không để LED_DEBUG sáng liên tục; nháy duty thấp.
- **Transceiver CAN (PA10 xuống thấp):** **ứng dụng** chỉ bật khi có config hợp lệ, qua `app_board_can_transceiver_normal(proof)`, và **trước** `HAL_CAN_Start`. **Bootloader** bật nó suốt thời gian chạy, bất kể config — CAN2 §5.1/§5.3 buộc vậy (module config trắng phải trả lời trên CAN2, mà chỉ có một transceiver); "im lặng" với bootloader nghĩa là chỉ phát `0x7E8` để trả lời `0x7E0`, không bao giờ `TH_*` (GĐ 5, câu hỏi 1, duyệt 2026-10-07).

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
| **0** | **Hợp đồng**: proposal, DBC v1.5, ID Map v1.3, CAN2 v1.5 | **ĐÃ DUYỆT 2026-10-05**, phương án A — `docs/VF_TH_CAN_Proposal.md` |
| **1** | Khung project, `app_config.h`, LED, im lặng trên CAN, `check_contracts` | **XONG 2026-10-07.** Build 0/0, `check_contracts` đã chứng minh fail được. Test board đạt: R8/R9 có lắp thật (PB6 = 5 V khi reset) **và firmware thắng được chúng (PB6 = 0 V khi chạy)**; im lặng trên CAN1 cả phát lẫn ACK; chip K8. Báo cáo `docs/VF_TH_Phase1_Report.md` §8 |
| **2** | Driver RS485 + Modbus RTU, đọc được nhiệt độ/độ ẩm | **XONG 2026-10-07.** Build 0/0, test host 362/362. Board: 25,4 °C vs phòng ~25 °C; 51 %RH, phản ứng khi thở; rút/cắm, khởi động không cảm biến, quét (cả đường "found" ở 9600) đều đạt; 10 phút 0 lỗi. **Mở:** độ chính xác RH (không có ẩm kế); DE chưa đo bằng oscilloscope (thay bằng phân tích, báo cáo §8.3); `latency_ms` chưa có số. Báo cáo `docs/VF_TH_Phase2_Report.md` |
| **3** | Phát `TH_Data` + `TH_Status` lên CAN1 (silent loopback trước) | **XONG 2026-10-07.** Build 0/0, test host 1577/1577 (11 đột biến đều bị bắt). Board: 61 s ⇒ 122 frame quay về khớp từng byte; thứ tự byte chứng minh (`0D 01` = 26,9 °C); frame chưa đọc `00 00 00 00 FF FF 00 01`; cổng `0xFF` ⇒ 0 frame; analyzer: không gì lên dây. Báo cáo `docs/VF_TH_Phase3_Report.md` |
| **4** | Kho config trong flash + phát thật trên bus | **XONG 2026-10-07.** Build 0/0, test host 2497/2497, `check_linker` chứng minh fail được. Board: page trắng ⇒ mã 3, không frame; config bench ⇒ **phát thật**, 206 / 206 frame, TEC 0; **mainboard `41ad090` ghi `.blf`, giải mã DBC v1.5**; CRC trên chip = CRC trên PC. **Mở: đo lại PA10 lúc phát** (báo 3,3 V, mâu thuẫn với frame trên dây và `stb_level` 0 — báo cáo §6.1). Báo cáo `docs/VF_TH_Phase4_Report.md` |
| **5** | Bootloader + giao thức CAN2 | **kế hoạch duyệt 2026-10-07** (`docs/VF_TH_Phase5_Plan.md`, cả 5 câu hỏi). **Phía ứng dụng xong** (tách `app_cfg_store` / `app_cfg_boot`, VTOR, transceiver trước `Start`, `check_linker` hai chế độ). **Chờ người dùng tạo `VF_TempHumi_Bootloader`** theo `docs/VF_TH_IOC_Guide.md` Phần B; code bootloader viết sau |
| 6 | Cờ lỗi, self-test, tích hợp với `VF_Main` | chưa làm |

**Giai đoạn 0 chặn tất cả.** ID và layout signal ràng buộc firmware của cả 10 module; đổi về sau nghĩa là nạp lại tất cả.

### 7.1 Đã chốt — đừng làm lại, đừng "sửa"

| Điều | Chốt | Ở đâu |
|---|---|---|
| Thứ tự khởi động | `app_board_early_guard()` ở `USER CODE 1`, **trước `HAL_Init`**: PB6 LOW rồi PA10 HIGH. B3 lặp lại ở `USER CODE 2` qua `app_board_enter_safe_state()` | `main.c`, `app_board.c` |
| Hàm có điều kiện / bị cấm | Hàm đưa transceiver sang Normal có từ GĐ 4 và **đòi config làm bằng chứng** (`app_board_can_transceiver_normal(const app_cfg_t *proof)` tự gọi `app_cfg_valid`). **Không có hàm export nào bật DE**: `rs485_drive_bus()` là `static` trong `drv_rs485.c`; nhả bus thì export (`app_board_rs485_release`) vì luôn an toàn | `app_board.h`, `drv_rs485.c` |
| LED | Khởi động 50 ms / 500 ms (10%). **Đang đo 50 ms / 5000 ms (1%)** khi số đo tươi. Mã N: 50 ms sáng, 250 ms tắt, nghỉ 2000 ms, tối đa 8. Mã lỗi **chốt** thắng mã **trạng thái sống**, cả hai thắng mẫu yên. Duty kiểm ở thì biên dịch. **Cùng ngôn ngữ với module góc** | `app_config.h`, `app_led.c` |
| Mã lỗi | `APP_FAULT_NONE/HAL_INIT/SENSOR` = 0/1/2. Mã 1 **chốt** (`app_fault_raise`, mã đầu tiên được giữ). **Mã 2 KHÔNG chốt** — trạng thái sống do `app_th` đặt qua `app_led_set_status`, không bao giờ truyền vào `app_fault_raise` (GĐ 2, câu hỏi 1, A). Dự kiến 3 = config sai CRC (trùng module góc). Số = số lần nháy. Chỉ thêm vào cuối | `app_fault.h` |
| DE (GĐ 2) | Bật nhận **trước** khi phát; DE lên ngay trước `HAL_UART_Transmit_IT`; **DE xuống trong `HAL_UART_TxCpltCallback`** (= cờ TC, sau bit stop). TC không tới trong 50 ms ⇒ hạ cưỡng bức, đếm `MB_ERR_TX`. Lỗi UART ⇒ hạ ngay trong ngắt | `drv_rs485.c` |
| Giao dịch Modbus (GĐ 2) | Một lần đọc 2 thanh ghi từ `0x0000` (RH rồi T, datasheet tr. 7), mỗi 2 s, lần đầu sau 2 s. Kết thúc khung theo `expected_len` hoặc lặng > 8 ms; không byte nào trong 200 ms ⇒ TIMEOUT. **Không dựa vào RTO phần cứng.** Mọi lối ra qua `th_finish` ⇒ **đúng một** bộ đếm | `app_th.c` |
| Tuổi số đo (GĐ 2) | Từ reply **hợp lệ** cuối; 0,1 s; `0xFFFF` = chưa đọc; bão hoà `0xFFFE`. **STALE ⇔ `DataAge` > 60** (xét trên con số được phát, để log tự nhất quán: 6099 ms tươi, 6100 ms STALE). NEVER_READ không kéo theo STALE | `app_meas.c` |
| Số ngoài dải (GĐ 2) | Giữ số, bật `RANGE`, tuổi về 0 (câu hỏi 3, A) | `app_meas.c` |
| Quét chẩn đoán (GĐ 2) | Một lần mỗi lần khởi động, sau 5 lần hỏi không có số hợp lệ: 4800 / 9600 / 2400 × `0xFF` đọc `0x07D0`. **Chỉ báo vào `g_diag.th.scan`, không bao giờ chấp nhận, quay về 4800** (câu hỏi 2, A) | `app_th.c` |
| Không ghi cảm biến | Firmware **chỉ đọc**, mãi mãi. `drv_modbus` chỉ có hàm 0x03 | `drv_modbus.h` |
| Console | `g_diag` (magic `THD1` = `0x54484431`) đọc bằng Live Expressions. `g_diag.th.count[]` đánh số theo `mb_status_t`; `g_diag.can` từ GĐ 3 | `app_diag.h`, `app_th.h`, `app_can.h` |
| CAN1 (GĐ 3) | **Silent loopback là hằng số của GĐ 3**: `app_can_init` gán `CAN_MODE_SILENT_LOOPBACK` rồi gọi lại `HAL_CAN_Init` trước `HAL_CAN_Start` (không sửa file sinh ra). Filter nhận mọi ID → FIFO0, đọc bằng thăm dò, không ngắt CAN. Hỏng ⇒ `APP_FAULT_CAN` = 5, chốt, **không fatal** | `app_can.c` |
| Frame (GĐ 3) | `TH_Data` + `TH_Status` **mỗi 1000 ms, cảm biến sống hay chết** (hợp đồng: `DataAge` nói sự thật). **Một tick chụp, hai frame** ⇒ `StatusFlags` == `Flags`. Chưa đọc ⇒ T = RH = 0, `DataAge` `0xFFFF`, `NEVER_READ` (câu hỏi 1, A). **`Counter` +1 mỗi khe 1 s** khi cổng mở, dù mailbox có nhận hay không (câu hỏi 2, A). Mailbox đầy ⇒ bỏ, `tx_full++`, không phát lại. Tên trường theo **DBC** (`StatusFlags`, `TxPeriod`), không theo proposal §5.2 | `app_can.c`, `app_frame.c` |
| `node_id` | **Chỉ từ config đã kiểm** (`app_cfg_active()`, `NULL` ⇒ `0xFF`). Hằng `APP_CAN_NODE_ID_PHASE3` của GĐ 3 **đã xoá ở GĐ 4**; `node_id_source` 1 bỏ, không tái dùng. Cổng `app_frame_node_ok` (= `APP_NODE_ID_OK`) đứng trước mọi thứ chạm bxCAN | `app_cfg_store.c`, `app_can.c` |
| Layout vs DBC | `tests/test_dbc_layout.c` **đọc thẳng `contracts/*.dbc`** và so từng signal với bit firmware thật sự ghi (mặt nạ toàn 1 + mẫu `0x12A5` cho thứ tự byte). DBC lên version ⇒ test này phải chạy lại và đạt | `tests/` |
| `Error_Handler` | Lấy LR → tắt ngắt → `MX_GPIO_Init()` lại → `app_fault_fatal(HAL_INIT, LR)` → nháy mã 1 bằng polling `SysTick COUNTFLAG` | `main.c` |
| HardFault | về trạng thái an toàn (RS485 nhả, transceiver Standby, LED tắt) rồi treo | `stm32f3xx_it.c` |
| Tên firmware | `s_fw_id` trong image (không có console). Bump `APP_FW_PHASE` mỗi phase | `main.c`, `app_config.h` |
| Bộ nhớ | 64 KB flash / 12 KB SRAM / 4 KB CCM, xác nhận từ database CubeMX. **Bản đồ flash: bootloader 12 KB @ `0x08000000` · ứng dụng 50 KB @ `0x08003000` (GĐ 5; tới đó link ở `0x08000000`) · config 2 KB @ `0x0800F800`** (page F3 = 2 KB). GĐ 1–3 ghi nhầm 52 KB (quên page config) — sửa ở GĐ 4. `_Static_assert` 12 + 50 + 2 = 64 | `app_config.h` |
| Linker (GĐ 4) | `STM32F303K8TX_FLASH.ld`: `FLASH LENGTH = 50K` — **người dùng sửa tay** (file do CubeMX sinh). `tools/check_linker.sh` chạy trong `makefile.init`: vùng FLASH với tới `APP_CFG_FLASH_ADDR` ⇒ **build đỏ**, gọi tên file. Một lần Generate trả về 64K sẽ bị bắt ở đây | `makefile.init`, `tools/` |
| Config (GĐ 4) | 16 byte, magic `"TCF1"` = `0x31464354`, version 1, `node_id`, `module_type`, 4 byte `reserved` = 0, CRC-32 IEEE trên 12 byte đầu. **Không có trường chu kỳ** (cố định 1000 ms). Kiểm theo thứ tự: trắng → magic → version → CRC → loại → node → reserved; `app_cfg_check` trả lý do đầu tiên. `app_cfg` **thuần** — bootloader GĐ 5 link chính file này | `app_cfg.h/.c` |
| Hai trạng thái CAN (GĐ 4) | Config hợp lệ ⇒ NORMAL, **`AutoRetransmission` + `AutoBusOff` ENABLE** (`.ioc` để cả hai DISABLE — trên bus thật `0x400` thua arbitration trước mọi `0x2xx`/`0x3xx`), filter danh sách `0x010 + node_id`, transceiver Normal qua `app_board_can_transceiver_normal(proof)` **TRƯỚC** `HAL_CAN_Start` (GĐ 5 sửa: GĐ 4 để sau — bxCAN có thể nhập bus giữa frame của module khác và phát cờ lỗi). Không hợp lệ (kể cả trắng) ⇒ silent loopback, standby, cổng đóng, **LED mã 3** chốt, không fatal. **Mỗi loại frame tối đa một frame chờ trong mailbox**: còn chờ ⇒ `AbortTxRequest`, `tx_aborted++` | `app_can.c`, `app_board.c` |
| Công tắc bench (GĐ 4) | `APP_CFG_BENCH_WRITE`, `APP_CFG_BENCH_ERASE`, `APP_BENCH_CAN_LOOPBACK` — **mặc định 0**. Bật ⇒ đúng một `#warning` (ở `app_cfg_store.c`) + cờ `g_diag.cfg` + tag trong `s_fw_id`. WRITE và ERASE cùng 1 ⇒ `#error`. Đến GĐ 5, bootloader thay chúng | `app_config.h` |

### 7.2 Sự thật phát hiện được, không nằm ở chỗ khác

- **`GND12` không nối với `GND` trên board** — xem §4.2. Hệ data logger độc lập: nguồn 12 V và nguồn 5 V có chung âm không — cần biết trước khi lắp đặt thật.
- **`VF_Main` không có bản nào của `VF_TH_CAN_Proposal.md`** (module góc thì có, ở `VF_Main/docs/VF_Angle/`). Bản gốc thực tế là `docs/` của repo này — nằm ngoài vùng `check_contracts` canh.
- Hai lỗi chữ trong hợp đồng, không ảnh hưởng firmware: proposal §5.3 ghi `0x040 + node_id` (đúng: `0x010 + node_id`); CAN2 v1.5 bảng §4.1 vẫn ghi `0x03` *chưa đặt tên*. Ghi ở `contracts/README.md`.
- `USART1` trên F303x8 lấy clock từ **PCLK1 = 36 MHz** (`RCC_USART1CLKSOURCE_PCLK1` trong `SystemClock_Config`), không phải PCLK2.
- Datasheet MAX485 (`MAX481.PDF` trang 2): VIH của DE/RE̅ = 2,0 V ⇒ PB6 push-pull 3,3 V đủ để bật driver dù R8/R9 kéo lên 5 V.
- **(GĐ 2) Cả 11 khung ví dụ trong datasheet SHT có CRC đúng** — dùng làm vector test (`tests/test_drv_modbus.c`). Nhiệt độ âm là bù hai.
- **(GĐ 2) Thiếu 12 V trông y hệt lỗi dây**: mọi giao dịch TIMEOUT, LED mã 2. Đã gặp ở lần chạy đầu. Quy trình test GĐ 2 có bài 0: đo 12 V ở J1 và chung âm trước.
- **(GĐ 2) Người dùng có bộ USB–RS485 trên PC** — đặt được baud/địa chỉ cảm biến (đã dùng cho bài 11). Firmware vẫn không bao giờ ghi cảm biến.
- **(GĐ 2) DE hạ muộn nhất ~25–30 µs sau bit stop** (ước lượng từ `objdump`, `-O0`); slave Modbus không được trả lời trước 7,3 ms. Chưa đo bằng oscilloscope.
