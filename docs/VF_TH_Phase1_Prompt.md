# VF_TempHumi_Module — prompt giai đoạn 1: khung project

> **Trạng thái: đã thực hiện 2026-10-05** — code + build xong, chờ test board. Kết quả và các chỗ lệch có chủ đích (early guard trước `HAL_Init`; B6 #3 tách thành bài 8a/8b/8c) ở `docs/VF_TH_Phase1_Report.md`.

Đây là prompt cho phiên Claude Code làm việc trong repo `VF_TempHumi Module`.
**Đọc `CLAUDE.md` trước, rồi làm theo file này.**

---

## Điều kiện bắt buộc trước khi bắt đầu

Dừng lại và hỏi người dùng nếu bất kỳ dòng nào dưới đây chưa đúng:

| # | Điều kiện | Kiểm thế nào |
|---|---|---|
| 1 | `docs/VF_TH_CAN_Proposal.md` đã **được duyệt**, và câu hỏi A/B ở §4 đã có câu trả lời | dòng "Trạng thái" ở đầu file |
| 2 | `contracts/` có **đúng** `VF_E-Drive_Data_Logger_v1.5.dbc`, **không** có bản `v1.4` sót lại | `ls contracts/` |
| 3 | `contracts/VF_Main_CAN_ID_Map.md` là **v1.3** | dòng đầu file |
| 4 | `.ioc` đã được người dùng tạo theo `docs/VF_TH_IOC_Guide.md` | tồn tại `VF_TempHumi_Module.ioc` |
| 5 | `Core/Src/gpio.c` **không** chứa lệnh tắt SWD | `grep -r SWJ_DISABLE Core/Src/` |
| 6 | Pinout trong CubeMX khớp `CLAUDE.md` §4.1 | `usart.c`: `PA9` + `PB7`; `gpio.c`: `PA10`, `PB6`, `PB0` |

**Điều kiện 1 không phải thủ tục.** ID và layout signal ràng buộc firmware của cả 10 module; đổi về sau nghĩa là nạp lại tất cả.

**Điều kiện 5 nếu sai thì dừng hẳn** và bảo người dùng quay lại `VF_TH_IOC_Guide.md` §A3. Nạp một image như thế sẽ giết SWD ở mỗi lần bật nguồn, và JP2 không có NRST.

**Điều kiện 6 cũng không phải thủ tục.** `CLAUDE.md` §4.1 **suy ra** bản đồ chân từ netlist chứ không đọc datasheet — datasheet STM32F303K8T6 không có trong repo. CubeMX là nguồn xác nhận.

---

## Lộ trình — để biết giai đoạn 1 nằm ở đâu

| GĐ | Nội dung | Kết quả kiểm được |
|---|---|---|
| 0 | Hợp đồng: proposal, DBC v1.5, ID Map v1.3 | ba repo cùng hash |
| **1** | **Khung project, `app_config.h`, LED, im lặng trên CAN và trên RS485** | **board sống, không phát gì, không giữ bus nào** |
| 2 | Driver RS485 + Modbus RTU, đọc được nhiệt độ và độ ẩm | số đọc được khớp với một ẩm kế đặt cạnh |
| 3 | `TH_Data` + `TH_Status` — **chỉ silent loopback** | layout chứng minh bằng loopback, chưa lên bus thật |
| 4 | Kho config trong flash, **lần phát thật đầu tiên** | mainboard ghi được vào `.blf`, giải mã bằng DBC v1.5 |
| 5 | Bootloader + giao thức CAN2 | đổi được `node_id` từ màn hình mainboard |
| 6 | Cờ lỗi, `FORCE_READ`, self-test, tích hợp | chạy cùng module rung và module góc trên một bus |

**Giai đoạn 1 cố tình không làm gì ngoài khung.** Lý do ở cuối file.

---

## Việc của giai đoạn 1

### B1. `tools/check_contracts.sh` và tích hợp vào build

Chép cách làm từ `VF_Angle_Module/tools/check_contracts.sh` — cùng script, cùng cơ chế.

Yêu cầu: **build fail khi hash lệch**, và thông báo lỗi **gọi tên file** nào lệch. Script so cả **tập tên file** lẫn SHA-256, nên một bản DBC cũ sót lại trong `contracts/` cũng phải làm fail.

Nếu không sửa được file CubeMX sinh ra mà không vi phạm `CLAUDE.md` §6.1 thì **dừng lại, giải thích, đề xuất** (ví dụ `Makefile` bao ngoài ở gốc repo), chờ duyệt.

**Phải chứng minh nó fail được**: sửa một byte trong một file `contracts/`, chạy build, dán output, rồi hoàn nguyên.

### B2. `Core/Inc/app/app_config.h`

Một nơi duy nhất cho mọi hằng số. **Mỗi hằng số lấy từ datasheet phải có comment ghi nguồn.**

```c
#define APP_FW_PHASE              "1"      /* in ra luc boot, bump moi phase */
#define APP_FW_VERSION            1u       /* vao TH_Status byte 0 */

/* --- Dinh danh tren bus, tu contracts/VF_Main_CAN_ID_Map.md --- */
#define APP_MODULE_TYPE           0x03u
#define APP_NODE_LABEL_MAX        10u
#define APP_NODE_ID_MIN           0x30u
#define APP_NODE_ID_MAX           0x39u
#define APP_NODE_ID_UNSET         0xFFu

#define APP_CAN_BITRATE           500000u
#define APP_CAN_BASE(node_id)     ((uint16_t)(0x100u + (node_id) * 0x10u))
#define APP_CAN_CMD_ID(node_id)   ((uint16_t)(0x010u + (node_id)))

/* --- Chu ky phat: CO DINH cho loai 0x3, xem Proposal muc 4 phuong an A --- */
#define APP_TX_PERIOD_MS          1000u
#define APP_STATUS_PERIOD_MS      1000u

/* --- Cam bien SHT RS485, datasheet "[Hshop.vn] SHT RS485 Modbus-RTU" --- */
#define SHT_SLAVE_ADDR_DEFAULT    0x01u    /* muc "Note": mac dinh 1 */
#define SHT_BAUD_DEFAULT          4800u    /* muc "Note": mac dinh 4800 */
#define SHT_FUNC_READ             0x03u    /* muc "Cau truc khung truyen" */
#define SHT_REG_HUMIDITY          0x0000u  /* muc "Bang thanh ghi" */
#define SHT_REG_TEMPERATURE       0x0001u  /* muc "Bang thanh ghi" */
#define SHT_REFRESH_MS            2000u    /* "Thoi gian lam moi": 2 s */
#define SHT_TEMP_MIN_X10          (-400)   /* "Moi truong lam viec": -40 C */
#define SHT_TEMP_MAX_X10          600      /* "Moi truong lam viec": +60 C */
#define SHT_RH_MIN_X10            0
#define SHT_RH_MAX_X10            1000

/* --- Khung Modbus RTU o 4800 8N1: 1 ky tu = 10 bit = 2083 us --- */
#define MB_CHAR_US                2083u
#define MB_SILENCE_US             ((MB_CHAR_US * 7u) / 2u)  /* 3.5 ky tu */
#define MB_REPLY_TIMEOUT_MS       200u
#define MB_POLL_PERIOD_MS         SHT_REFRESH_MS            /* nhanh hon la vo nghia */

/* --- Tuoi so do: nguong STALE = 3 lan lam moi --- */
#define APP_TH_STALE_MS           6000u

#define APP_LED_DUTY_PCT_MAX      10u
```

### B3. Khung `main()` — và thứ tự rất quan trọng

```c
/* USER CODE BEGIN 2 */
/* TRUOC MOI THU KHAC: nha bus RS485.
   Board keo DE/RE len +5 V qua 5 kOhm (CLAUDE.md 4.3), nen tu luc co dien
   toi day, MAX485 dang PHAT va giu bus. */
HAL_GPIO_WritePin(ENA_RS485_GPIO_Port, ENA_RS485_Pin, GPIO_PIN_RESET);

/* Transceiver CAN o Standby. Module chua cau hinh KHONG duoc ACK tren bus do:
   trung node_id lam sap bus, khong chi lam roi log. */
HAL_GPIO_WritePin(EN_CAN1_GPIO_Port, EN_CAN1_Pin, GPIO_PIN_SET);
/* USER CODE END 2 */
```

Vòng lặp chính: nháy LED theo nhịp, duty ≤ `APP_LED_DUTY_PCT_MAX`. Không gì khác.

### B4. Một module báo lỗi tối thiểu

`app_fault.c/.h`: ghi nhận lỗi, đếm, và **nháy LED theo mã**. Mỗi `return` sớm trong một đường dữ liệu phải tăng một bộ đếm — xem `CLAUDE.md` §6.3.

### B5. Báo cáo bộ nhớ

`arm-none-eabi-size`, và **xác nhận dung lượng flash/RAM thật của STM32F303K8T6 trong CubeMX**, ghi vào `app_config.h` làm comment. Phase 5 sẽ cần 12 KB cho bootloader.

### B6. Ba phép đo trên board — phần quan trọng nhất của giai đoạn này

Giai đoạn 1 không có chức năng nào để test. Nó tồn tại để **chứng minh ba điều mà mọi phase sau đều dựa vào**:

| # | Đo gì | Mong đợi | Nếu sai |
|---|---|---|---|
| 1 | `PB6` so GND, sau khi firmware chạy | **~0 V** | Board khác netlist, hoặc GPIO chưa được đặt. **Ghi lại sự thật đo được**, đừng tin netlist |
| 2 | `PA10` so GND | **~3,3 V** (Standby) | Module sẽ ACK trên bus đo trước khi được cấu hình |
| 3 | `CAN_H`/`CAN_L` khi cắm vào bus có traffic | Module **không ACK**, mainboard báo lỗi ACK | Cổng im lặng chưa đóng |

---

## Tiêu chí nghiệm thu giai đoạn 1

| # | Tiêu chí |
|---|---|
| 1 | Build **0 error**, mọi warning được giải thích |
| 2 | `check_contracts.sh` **đã được chứng minh là fail được** (dán output) |
| 3 | Báo cáo flash/RAM, và dung lượng thật của MCU đã xác nhận |
| 4 | LED nháy, duty ≤ 10% |
| 5 | **Ba phép đo ở B6 đạt** |
| 6 | Quy trình test viết xong, ở `docs/VF_TH_Phase1_Test.md` |

### Quy trình test phải bao gồm ít nhất

1. Nạp firmware, xác nhận SWD vẫn kết nối được **sau khi rút và cắm lại nguồn** (bẫy §A3).
2. Ba phép đo B6, kèm chỗ đặt que đo.
3. Cắm module vào bus CAN1 **đang có traffic** và xác nhận mainboard **không** thấy node mới ở `0x400`–`0x49F`.
4. Rút cảm biến khỏi J1 rồi cắm lại — xác nhận không có gì treo (giai đoạn 1 chưa nói chuyện với cảm biến, nên "không có gì xảy ra" chính là kết quả đúng).

---

## Những gì giai đoạn 1 CỐ TÌNH không làm

| Không làm | Vì sao |
|---|---|
| Không nói chuyện với cảm biến | Driver Modbus là phase 2. Trộn vào đây thì khi có lỗi không biết là khung project hay là driver |
| Không phát một frame CAN nào | Module chưa có `node_id` hợp lệ. Phát bừa một ID là cách nhanh nhất làm sập bus đo |
| Không có bootloader | Phase 5. Phân vùng flash đã chốt ở `VF_TH_IOC_Guide.md` §A9 nên không phải dời về sau |
| Không có float, không có lọc | Số của cảm biến đã hiệu chuẩn sẵn và đi thẳng lên CAN cùng đơn vị ×10 |

---

## Ba điều phiên này nên biết trước

### 1. Cái bẫy DE của RS485

`VF_TH_IOC_Guide.md` §A8. Phase 2 sẽ đâm vào nó; đọc trước thì đỡ một vòng.

### 2. Vì sao `DataAge` tồn tại

`VF_TH_CAN_Proposal.md` §5.1. Tóm tắt: module rung từng phát một giá trị nhiệt độ **đóng băng từ lúc khởi động** suốt nhiều tuần mà không ai biết, vì một giá trị cũ trông y hệt một giá trị mới. Loại module này có đúng một cách hỏng thật sự nguy hiểm, và đó là nó.

### 3. Hai module anh em đã đi trước

```
VF_Vibration_Module/   loai 0x1  — mau cho check_contracts, bootloader, kho config
VF_Angle_Module/       loai 0x2  — mau gan nhat; doc CLAUDE.md muc 7.3 "Da chot"
```

Đọc để **chép cách làm**, không phải để chép code: MCU khác (F303 có FPU, F103 không), cảm biến khác (Modbus UART thay vì SPI), và nhịp khác (1 Hz thay vì 416 Hz).
