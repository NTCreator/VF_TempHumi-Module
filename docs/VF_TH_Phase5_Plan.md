# Giai đoạn 5 — Kế hoạch: bootloader và giao thức cấu hình CAN2

**Trạng thái: CHỜ DUYỆT** (2026-10-07). Chưa viết dòng code nào. Năm câu hỏi ở mục 11. Hướng dẫn tạo project bootloader: **`docs/VF_TH_IOC_Guide.md` Phần B** — làm được ngay, song song với việc duyệt.

Căn cứ: CAN2 Protocol **v1.5** §3, §4, §4.1, §4.2, §5 (cả năm bất biến, kể cả câu v1.4 "config không hợp lệ ⇒ ở lại"); ID Map v1.3; kết quả GĐ 4 (`app_cfg` thuần, CRC chip = CRC PC); module góc **6a-2 và 6a-2b** (`VF_Angle_Module/docs/VF_Angle_Phase6a2_Plan.md`, `…_Report.md`, `…Phase6a2b_Plan.md`) — chép cách làm, không chép code.

---

## 0. Mục tiêu — kiểm được trên board

1. **Hai image.** Bootloader ở `0x08000000` (≤ 12 KB, `-Og`), ứng dụng ở **`0x08003000`** (50 KB), config ở `0x0800F800` (2 KB).
2. **Chính sách lúc cấp nguồn** (CAN2 §5, bất biến 5, v1.5):
   - không có ứng dụng hợp lệ ⇒ **ở lại mãi** (đường cứu);
   - **config không hợp lệ ⇒ ở lại mãi** (module mới / page trắng — cấu hình được mà không canh giờ);
   - còn lại ⇒ nghe `0x7E0` **≥ 1000 ms**; nghe thấy ⇒ ở lại (phiên); im ⇒ nhảy sang ứng dụng.
3. **Giao thức `0x01`–`0x30` cho loại `0x03`**, trả đúng mã (`0x01` lệnh không có, `0x02` tham số sai, `0x03` lỗi flash).
4. **RS485 được nhả từ micro-giây đầu tiên của bootloader** — và **vẫn được giữ thấp qua cú nhảy** sang ứng dụng (mục 2.5).
5. **Ghép với mainboard thật** qua J2: đặt nhãn 1…10 trên màn hình, `COMMIT`, module phát đúng `0x4x0` / `0x4x2` trên CAN1.
6. **SWD sống sót** qua mọi lần cấp nguồn, với bootloader chạy trước.

Nạp firmware qua CAN2 (`0x40`+) **không** nằm trong GĐ 5: hợp đồng chưa đặc tả (CAN2 §7). Ứng dụng vẫn nạp bằng SWD.

---

## 1. Dữ kiện đã kiểm

| Dữ kiện | Nguồn | Hệ quả |
|---|---|---|
| 500 kbps, request **`0x7E0`**, response **`0x7E8`**, luôn 8 byte (đệm `0x00`), **trả lời trong 50 ms** | CAN2 §3 | cùng bit timing với CAN1 — `.ioc` bootloader giống phần A |
| Loại `0x03`: `SET_TX_PERIOD` **tập rỗng — mọi giá trị `0x02`**; `SET_FULL_SCALE`, `SET_GYRO_FS` **không hỗ trợ** | CAN2 §4.2 | câu hỏi 4 |
| `0x01` = "loại module này không có lệnh đó"; `0x02` = "có lệnh, giá trị ngoài tập con" | CAN2 §4, bảng dưới `0x14` | |
| `SET_NODE_ID` và `SET_MODULE_TYPE` đều kiểm `(node_id >> 4) == module_type && (node_id & 0x0F) <= 9`; `COMMIT` với cặp không nhất quán ⇒ `0x02`, không ghi | CAN2 §4.1 | = `app_cfg_check()` của GĐ 4 |
| **Config không hợp lệ ⇒ bootloader ở lại vĩnh viễn**; phép kiểm phải **đúng là hàm ứng dụng dùng**; đổi định dạng config = cập nhật bootloader cùng lúc | CAN2 §5 bất biến 5 (v1.4) | link `app_cfg.c`, không chép |
| J2 cấp 5 V **thường trực**: module đã cấu hình rời bootloader ~1 s sau khi chạm connector. Mainboard hỏi lặp `HELLO` tới **10 s**; người vận hành bấm reset trên module | CAN2 §5.5 (v1.4) | SW1 là thao tác chuẩn để vào bootloader của module đã cấu hình |
| UID: **`UID_BASE = 0x1FFFF7AC`** (`stm32f303x8.h`), đọc qua `HAL_GetUIDw0/1/2()` | CMSIS, HAL | không đoán địa chỉ |
| ⛔ **`HAL_DeInit()` trên F3 force-reset APB1, APB2 và AHB — các cổng GPIO nằm trên AHB** | `stm32f3xx_hal.c` | **gọi nó trước khi nhảy là PB6 thả nổi ⇒ R8/R9 kéo lên ⇒ MAX485 PHÁT** cho tới khi ứng dụng chạy early guard. Module góc gọi `HAL_DeInit` (board đó không có MAX485). **Ở đây không gọi** — mục 2.5 |
| `HAL_RCC_DeInit()` về HSI, có timeout theo `HAL_GetTick`, **không đụng** thanh ghi bật clock / reset ngoại vi | `stm32f3xx_hal_rcc.c` | gọi **trước** khi tắt SysTick; GPIO vẫn được lái |
| `g_pfnVectors` là `.global` trong startup | `startup_stm32f303k8tx.s` | ứng dụng đặt `VTOR` từ symbol, không viết tay địa chỉ |
| Page F3 2 KB: bootloader = 6 page, ứng dụng = 25 page, config = 1 page | GĐ 4 | |
| `VF_Main` `41ad090` có cấu hình loại `0x3` (nút TempHumi → `app_guidata_cfg_type(3)`, không nút chu kỳ) | repo `VF_Main`, GĐ 4 | bài mainboard làm được không cần nạp lại |

---

## 2. Thiết kế

### 2.1 Bản đồ flash — một nguồn hằng số cho cả hai image

`app_config.h` (thuần, bootloader include được) thêm:

```c
#define APP_BOOT_BASE     0x08000000u   /* 12 KB */
#define APP_BOOT_SIZE     0x3000u
#define APP_IMAGE_BASE    0x08003000u   /* 50 KB */
#define APP_IMAGE_SIZE    0xC800u
/* APP_CFG_FLASH_ADDR 0x0800F800, 2 KB - since phase 4 */
```

`_Static_assert`: ba vùng nối tiếp, không chồng, cộng đúng 64 KB. Bootloader nhảy tới `APP_IMAGE_BASE` — **cùng hằng số** `check_linker.sh` đọc để kiểm linker ứng dụng.

### 2.2 Bootloader — một trang

```
reset
 |- app_board_early_guard()       PB6 LOW, PA10 HIGH - TRUOC HAL_Init (code GD 1, link tu ung dung)
 |- HAL_Init, SystemClock_Config, MX_GPIO_Init, MX_CAN_Init
 |- transceiver Normal (boot_board - KHONG doi config: CAN2 5.1, 5.3)
 |- HAL_CAN_Start; filter: dung 0x7E0, frame du lieu chuan
 |     Start hong -> co ung dung + config hop le ? nhay : o lai (LED ma 5)
 |
 |- boot_policy(app_present, app_cfg_check(page)):
 |     ung dung khong co that      -> O LAI MAI   (duong cuu)
 |     config khong hop le         -> O LAI MAI   (chua cau hinh)
 |     con lai                     -> NGHE 1000 ms
 |
 '- NGHE: nghe thay 0x7E0 -> PHIEN (o lai toi RESET / tat-bat)
          im lang          -> NHAY
```

- **"Ứng dụng có thật"**: MSP đầu image nằm trong SRAM (`0x20000000`…`0x20003000`), reset vector **lẻ** (Thumb) và nằm trong vùng ứng dụng. Vùng trắng (`0xFFFFFFFF`) trượt ngay.
- **Không ngắt CAN nào**; FIFO0 thăm dò. Ít thứ phải gỡ trước khi nhảy.
- **Transceiver Normal suốt thời gian bootloader chạy, bất kể config** — hợp đồng buộc vậy: module config trắng **phải trả lời** trên CAN2 (§5.1, §5.3), mà module chỉ có một bxCAN và một transceiver, không biết mình đang cắm vào bus nào. "Im lặng" với bootloader nghĩa là: **không bao giờ phát frame không được hỏi**; chỉ phát `0x7E8` để trả lời `0x7E0`; **không bao giờ** `TH_*`. Trên CAN1 không ai gửi `0x7E0`, nên bootloader chỉ ACK như mọi node nghe. Hàm làm việc này nằm **trong project bootloader**, ứng dụng không link nó; quy tắc "chỉ config hợp lệ mới kéo PA10 xuống" vẫn là tính chất của `app_board_can_transceiver_normal()` phía ứng dụng — câu hỏi 1.

### 2.3 Giao thức — `boot_svc` (thuần, không HAL)

Một bản làm việc trong RAM (nạp từ flash lúc khởi động); `SET_*` chỉ đổi RAM; mọi request đều có trả lời.

| Mã | Lệnh | Luật cho loại `0x03` |
|---|---|---|
| `0x01` | `HELLO` | `[2]` proto **1**, `[3]` **`0x03`**, `[4]` `node_id` của **config đang lưu** (`0xFF` nếu `app_cfg_check` ≠ OK), `[5..7]` phiên bản **bootloader** |
| `0x02` / `0x03` | `GET_UID_LO/HI` | byte 0–5 / 6–11 của UID 96 bit (`HAL_GetUIDw0/1/2`, little-endian từng word) |
| `0x10` | `SET_NODE_ID` | `app_cfg_node_id_ok()` **và** nibble cao = `module_type` của bản làm việc ⇒ `0x00`, không thì `0x02` |
| `0x11` | `SET_MODULE_TYPE` | chỉ nhận **`0x03`** — firmware này chỉ làm được loại nhiệt–ẩm; kiểm lại cặp với `node_id` đang giữ |
| `0x12` | `SET_TX_PERIOD` | **luôn `0x02`** — CAN2 §4.2 ghi rõ |
| `0x13` | `SET_FULL_SCALE` | **`0x01`** — câu hỏi 4 |
| `0x14` | `SET_GYRO_FS` | **`0x01`** — câu hỏi 4 |
| `0x20` | `COMMIT` | `app_cfg_check(bản làm việc)` phải OK, không thì `0x02` và **không ghi**. Ghi qua đúng `app_cfg_store_write` (seal → xoá → ghi → đọc lại). Hỏng ⇒ `0x03` |
| `0x30` | `RESET` | trả lời **trước**, đợi frame rời mailbox (có giới hạn), rồi `NVIC_SystemReset()` (CAN2 §5 bất biến 4) |
| khác, kể cả `0x40`+ | | `0x01` |

**Luật tham số không viết lại lần nào** — đó là `app_cfg_node_id_ok`, `app_cfg_module_type_ok`, `app_cfg_check` của GĐ 4, đã có 920 kiểm tra host và CRC đã khớp chip.

### 2.4 Code dùng chung — link, không chép; tách phụ thuộc trước

Bootloader **link ba file** của ứng dụng (IOC Guide B6): `app_cfg.c`, `app_cfg_store.c`, `app_board.c`.

`app_cfg_store.c` hiện đọc `g_diag`, gọi `app_fault_raise`, và mang ba `#warning` công tắc bench — bootloader không có những thứ đó. **Tách** (như module góc 6a-2):

| File | Việc | Dùng ở |
|---|---|---|
| `app_cfg_store.[ch]` | chỉ thao tác flash: `read`, `write`, `erase`, **trả mã trạng thái** | **cả hai image** |
| `app_cfg_boot.[ch]` **mới** | `app_cfg_boot()`, `app_cfg_active()`, `g_diag.cfg`, mã 3, công tắc bench + `#warning` — **chuyển nguyên**, hành vi không đổi | chỉ ứng dụng |

`app_board.c` link được nguyên: nó chỉ dùng HAL GPIO và nhãn `main.h` (bootloader `.ioc` dùng **cùng nhãn** — IOC Guide B2), và `app_cfg_valid` (đã link). Bootloader dùng `app_board_early_guard()`, `app_board_enter_safe_state()`, `app_board_led_set()`; **không bao giờ** gọi `app_board_can_transceiver_normal()`.

Code **chỉ của bootloader** nằm trong `VF_TempHumi_Bootloader/Core/{Inc,Src}/app/`: `boot_svc` (thuần), `boot_policy.h` (thuần, chỉ header), `boot_can`, `boot_jump`, `boot_board` (transceiver vô điều kiện, LED, UID), `boot_config.h` (phiên bản, hằng LED).

### 2.5 Cú nhảy — **không `HAL_DeInit`**

1. **PA10 → standby**, PB6 vẫn **thấp** (chỉ ghi ODR — không đụng mode).
2. `HAL_CAN_Stop` + `HAL_CAN_DeInit` (MspDeInit trả PA11/PA12 về input; TXD của TJA1044 không còn ai lái, mà transceiver đã standby).
3. `HAL_RCC_DeInit()` — về HSI, **khi SysTick còn chạy** (nó dùng `HAL_GetTick` cho timeout).
4. Tắt SysTick bằng tay.
5. Xoá mọi ngắt đang bật / đang chờ trong NVIC.
6. `SCB->VTOR = APP_IMAGE_BASE`.
7. Nạp MSP rồi `bx` trong **cùng một khối asm**, để trình biên dịch không chèn `push` vào giữa.

**Không có bước nào reset cổng GPIO**: PB6, PA10, PB0 **được lái liên tục** từ `app_board_early_guard()` của bootloader tới `app_board_early_guard()` của ứng dụng. Cửa sổ MAX485 phát chỉ còn đúng cửa sổ phần cứng lúc cấp nguồn (reset → bootloader `main`) — cùng độ dài như GĐ 1–4.

### 2.6 Ứng dụng — năm thay đổi

| # | Thay đổi | Vì sao |
|---|---|---|
| 1 | **Linker: `ORIGIN = 0x8003000`, `LENGTH = 50K`** — bạn sửa tay, **cùng lúc** với lần nạp bootloader đầu tiên | câu hỏi 3 |
| 2 | `SCB->VTOR = (uint32_t)g_pfnVectors` ở `USER CODE 1`, **sau** `app_board_early_guard()`, **trước** `HAL_Init()` (SysTick là ngắt đầu tiên). `g_diag.app_base` ghi lại — phải đọc `0x08003000` | debug launch từ CubeIDE chạy thẳng ứng dụng, bỏ qua bootloader: ứng dụng phải tự đặt VTOR. Lấy từ symbol nên đúng theo cấu tạo |
| 3 | **Transceiver Normal TRƯỚC `HAL_CAN_Start`** (vẫn qua bằng chứng config); `Start` hỏng ⇒ về standby | câu hỏi 2 — sửa một rủi ro của GĐ 4 |
| 4 | Tách `app_cfg_store` → + `app_cfg_boot` (2.4) | bootloader link được |
| 5 | `tools/check_linker.sh` kiểm **cả hai** linker script; `APP_FW_PHASE "5"` | |

**Vì sao thay đổi 3.** GĐ 4 bật transceiver **sau** `HAL_CAN_Start`. Khi `Start` chạy, transceiver còn standby nên RXD đứng yên ở mức lặn: bxCAN thấy 11 bit lặn và coi bus rảnh **ngay lập tức**. Rồi transceiver bật — **có thể giữa một frame** của module khác. bxCAN thấy một bit trội, tưởng là SOF, đọc tiếp, gặp lỗi nhồi bit / lỗi khuôn, và là node error-active **nó phát cờ lỗi 6 bit trội — phá frame của module khác trên bus đo**. Bật transceiver trước thì `Start` đợi 11 bit lặn **trên bus thật** rồi mới tham gia; trong lúc chờ, bxCAN còn ở init mode nên CANTX lặn, không gì lên dây. Các bài GĐ 4 chạy trên bus rảnh nên không thể thấy lỗi này. Bootloader module rung và ứng dụng module góc (từ 6a-2, quyết định 2) đều bật transceiver trước — kế hoạch 6a-2 của module góc §2.6.

### 2.7 LED của bootloader — cùng ngôn ngữ với module góc

| Trạng thái | LED (L2, PB0) |
|---|---|
| Cửa sổ nghe / phiên / đường cứu | **nháy đôi** mỗi giây: 50 sáng, 150 tắt, 50 sáng, 750 tắt (duty 10%) |
| **Chưa cấu hình, chưa ai nói gì** | **mã 3** — đúng nhịp ứng dụng dùng cho mã 3: cùng nghĩa "không có config hợp lệ", dù bootloader hay ứng dụng đang hiện |
| Chưa cấu hình, **đã nhận `0x7E0`** | chuyển sang **nháy đôi** — thấy ngay mainboard đã nói chuyện được |
| `Start` CAN hỏng, ở lại | **mã 5** (`APP_FAULT_CAN`) |

Duty kiểm ở thì biên dịch với `APP_LED_DUTY_PCT_MAX`; mã 3 và mã 5 kiểm với `app_fault.h`.

---

## 3. File

| File | | Việc |
|---|---|---|
| `Core/Inc/app/app_config.h` | sửa | bản đồ flash (2.1); `APP_FW_PHASE "5"` |
| `Core/{Inc,Src}/app/app_cfg_store.*` | sửa | chỉ flash, trả mã trạng thái |
| `Core/{Inc,Src}/app/app_cfg_boot.*` | **mới** | phần của ứng dụng, chuyển nguyên |
| `Core/Src/app/app_can.c` | sửa | transceiver trước `Start` |
| `Core/Src/main.c` | sửa, USER CODE | VTOR, `g_diag.app_base` |
| `STM32F303K8TX_FLASH.ld` (ứng dụng) | **bạn sửa** | `ORIGIN = 0x8003000` |
| `tools/check_linker.sh`, `makefile.init` | sửa | hai linker script |
| `VF_TempHumi_Bootloader/` | **bạn tạo** (IOC Guide B) | project, `.ioc`, linker 12K, `-Og`, ba file link, include path |
| `VF_TempHumi_Bootloader/makefile.init` | **mới** | `check_contracts` + `check_linker` |
| `VF_TempHumi_Bootloader/Core/{Inc,Src}/app/boot_*` | **mới** | 2.2–2.7 |
| `VF_TempHumi_Bootloader/Core/Src/main.c` | sửa, USER CODE | luồng 2.2 |
| `tests/test_boot_svc.c`, `tests/test_boot_policy.c`, `Makefile` | **mới / sửa** | mục 6 |
| `CLAUDE.md` §6.5, §7 | sửa | câu hỏi 1; trạng thái |

Không sửa `.ioc` nào, không sửa `contracts/`, không sửa file CubeMX sinh ra ngoài USER CODE (trừ hai linker script — bạn sửa).

## 4. API

```c
/* boot_svc.h - pure */
#define BOOT_SVC_REQ_ID  0x7E0u
#define BOOT_SVC_RSP_ID  0x7E8u
typedef struct {
  void    (*read_cfg)(app_cfg_t *out);
  uint8_t (*write_cfg)(app_cfg_t *cfg);      /* APP_CFG_WR_*: not OK -> 0x03 */
  void    (*read_uid)(uint8_t out12[12]);
} boot_svc_hooks_t;
typedef enum { BOOT_SVC_NONE = 0, BOOT_SVC_RESET = 1 } boot_svc_action_t;
void    boot_svc_init(const boot_svc_hooks_t *hooks);
uint8_t boot_svc_handle(const uint8_t *req, uint8_t len, uint8_t rsp[8]);   /* -> boot_svc_action_t */

/* boot_policy.h - pure, header only */
#define BOOT_POLICY_LISTEN             0u
#define BOOT_POLICY_STAY_RESCUE        1u   /* no application */
#define BOOT_POLICY_STAY_UNCONFIGURED  2u   /* config not valid */
uint8_t boot_policy_at_power_up(bool app_present, uint8_t cfg_state);
bool    boot_policy_app_present(uint32_t msp, uint32_t reset_vector);

/* app_cfg_store.h - shared, flash only */
void    app_cfg_store_read(app_cfg_t *out);
uint8_t app_cfg_store_write(app_cfg_t *c);   /* APP_CFG_WR_OK / _ERASE / _PROGRAM / _VERIFY / _LOCK */
uint8_t app_cfg_store_erase(void);

/* app_cfg_boot.h - application only, behaviour unchanged */
void             app_cfg_boot(void);
const app_cfg_t *app_cfg_active(void);
```

## 5. Ngân sách

| | Ước tính |
|---|---|
| **Bootloader** | **~9–11 KB ở `-Og`** — module rung (F103) 8,7 KB; HAL F3 lớn hơn. **Trần 12 KB là rủi ro thật** — báo số sau lần build đầu; dự phòng `-Os` |
| Ứng dụng | ±200 B (VTOR, tách file) — vẫn ~55% của 50 KB |
| Thời gian khởi động | +≥ 1000 ms cửa sổ nghe trước khi ứng dụng chạy (module đã cấu hình) |
| `COMMIT` | một lần xoá page 2 KB + ghi 8 half-word, **phải trả lời trong 50 ms** (CAN2 §3). Thời gian xoá page F303 nằm trong datasheet chưa có trong repo — **đo ở bài 4**; mainboard thử lại 3 lần |

## 6. Test trên PC

| Test | Chứng minh |
|---|---|
| `test_boot_svc` — flash giả (cho hỏng theo lệnh), UID giả | `HELLO` với config trắng / hợp lệ (`[3]` = 3, `[4]` = `0xFF` / node); UID 12 byte đúng thứ tự; **`SET_NODE_ID` quét 256 giá trị ⇒ đúng `0x30`–`0x39`**; `SET_MODULE_TYPE` 1 / 2 / 4 ⇒ `0x02`, 3 ⇒ OK; **`SET_TX_PERIOD` mọi giá trị ⇒ `0x02`**; `0x13`, `0x14` ⇒ `0x01`; `SET_*` không `COMMIT` rồi init lại ⇒ flash không đổi; `COMMIT` khi chưa có node ⇒ `0x02`, **không gọi `write_cfg`**; `COMMIT` hợp lệ ⇒ `write_cfg` đúng một lần, `HELLO` thấy node mới; flash hỏng ⇒ `0x03`; `RESET` ⇒ OK **và** action `RESET`; `0x00`, `0x40`, `0x7F` ⇒ `0x01`; frame ngắn ⇒ đệm 0, vẫn trả lời |
| `test_boot_policy` | bảng đủ: không ứng dụng × mọi `cfg_state` ⇒ RESCUE; có ứng dụng × 7 lý do hỏng ⇒ UNCONFIGURED; có × OK ⇒ LISTEN; `app_present`: MSP ngoài SRAM / vector chẵn / vector ngoài vùng / `0xFFFFFFFF` ⇒ không có |
| `test_app_cfg`, `test_dbc_layout`, … | giữ nguyên — `app_cfg` không đổi |

## 7. Test trên board (phác thảo — bản đầy đủ sau khi code)

Công cụ: **CubeProgrammer** (nạp **hai** image, **không** Full chip erase — nó xoá luôn page config), analyzer (đóng vai mainboard: gửi `0x7E0`, nghe `0x7E8`), rồi mainboard thật qua J2.

| # | Bài | Mong đợi |
|---|---|---|
| 1 | Nạp bootloader + ứng dụng (`ORIGIN` mới), tắt/bật | LED **nháy đôi ~1 s** rồi mẫu của ứng dụng; `g_diag.app_base` = `0x08003000`; board còn config bench ⇒ phát `0x400` / `0x402` như GĐ 4 |
| 2 | **PB6 trong bootloader**: giữ module ở bootloader (bài 3 hoặc 5), đo PB6 | **≈ 0 V** suốt thời gian ở bootloader. Oscilloscope (tuỳ chọn): PB6 **không** nhảy lên lúc chuyển từ bootloader sang ứng dụng |
| 3 | **Đường cứu**: xoá vùng `0x08003000`–`0x0800F7FF` bằng CubeProgrammer, tắt/bật, gửi `HELLO` | nháy đôi **mãi**; `0x7E8` đủ trường. **Bài quan trọng nhất** |
| 4 | Toàn bộ giao thức bằng analyzer: `HELLO`, UID (đối chiếu CubeProgrammer), `SET_NODE_ID 0x25` ⇒ `0x02`, `0x33` ⇒ OK, `SET_TX_PERIOD 100` ⇒ `0x02`, `SET_FULL_SCALE 4` ⇒ `0x01`, `COMMIT`, `HELLO` thấy `0x33`, `RESET` (thấy `0x7E8` **trước** khi khởi động lại). Đo thời gian `COMMIT` → `0x7E8` | module phát **`0x430` / `0x432`**; < 50 ms |
| 5 | **Chưa cấu hình** (bench `ERASE` một lần): tắt/bật trên bus có analyzer | **mã 3, ở lại mãi**; analyzer: **không `0x7E8`, không `TH_*`** khi không ai hỏi; gửi `HELLO` ⇒ trả lời, LED sang nháy đôi |
| 6 | **Mainboard thật qua J2** (không nối CAN1): màn hình CẤU HÌNH chọn TempHumi, nhãn 1, `COMMIT`; rút ra, cắm CAN1 | mainboard báo OK, sổ kiểm kê có dòng mới; trên CAN1 module phát `0x400` / `0x402` |
| 7 | Tắt/bật 3 lần, ST-Link Connect | SWD vào được mỗi lần |
| 8 | Hồi quy GĐ 2–4 | đọc cảm biến, frame, filter, mất ACK |

---

## 8. Rủi ro

| Rủi ro | Cách chặn |
|---|---|
| **Bootloader quên PB6 ⇒ MAX485 giữ bus RS485 mãi** (module chưa cấu hình ở lại vĩnh viễn) | link `app_board.c`: **cùng** `app_board_early_guard()` đã đo ở GĐ 1; bài 2 đo PB6 khi đang ở bootloader |
| **PB6 thả nổi qua cú nhảy** | không `HAL_DeInit` (2.5); bài 2 |
| Nạp ứng dụng một mình sau khi dời `ORIGIN` ⇒ board tối hẳn | quy trình "hai image"; `boot_policy_app_present` giữ bootloader nếu vùng ứng dụng trắng |
| **Full chip erase xoá page config** | quy trình ghi rõ: không Full chip erase |
| Debug ứng dụng trong CubeIDE xoá mất bootloader | kiểm sau lần debug đầu: tắt/bật phải thấy nháy đôi |
| **Bootloader vượt 12 KB** | `-Og`; chỉ ba file link; `check_linker` bắt khi tràn; dự phòng `-Os` |
| `COMMIT` quá 50 ms | đo ở bài 4; mainboard thử lại 3 lần |
| Hai linker script lệch bản đồ | `check_linker` đọc **cùng** hằng số từ `app_config.h` cho cả hai |
| Cắm module vào J2 khi còn nối CAN1 | CAN2 §1: hai nguồn 5 V đối đầu — quy trình nhắc lại |

## 9. Cố tình KHÔNG làm

| | Vì sao |
|---|---|
| Nạp firmware qua CAN2 (`0x40`+) | CAN2 §7 chưa đặc tả ⇒ `0x01` |
| Lệnh `TH_Cmd` trên CAN1 (`PING`, `FORCE_READ`) | GĐ 6 |
| Ứng dụng nghe `0x7E0` rồi tự vào bootloader (B2 của module góc) | đưa giao thức CAN2 vào ứng dụng — hợp đồng §5 bất biến 1 cố ý giữ nó ngoài; và mở một đường reset giữa lúc đo nếu `0x7E0` lạc lên CAN1 |

## 10. Ghi lại cho GĐ 6

`TH_Cmd`: filter `0x040` đã có từ GĐ 4; `CmdSeq` ban đầu = 0 "chưa nhận lệnh nào"; `FORCE_READ` gọi vào lịch của `app_th`. Watchdog (cần sửa `.ioc` ⇒ bạn làm).

---

## 11. Cần bạn quyết

### Câu hỏi 1 — Bootloader bật transceiver suốt thời gian chạy, bất kể config

Hợp đồng buộc: module config trắng **phải trả lời** trên CAN2 (§5.1, §5.3), và nó chỉ có một transceiver. Bootloader chỉ phát `0x7E8` để trả lời `0x7E0`, không bao giờ `TH_*`. **Đề xuất: duyệt**, và viết lại CLAUDE.md §6.5 thành "**ứng dụng** không kéo PA10 xuống khi chưa có config hợp lệ; bootloader theo CAN2 §5". Giống module góc (quyết định 1 của 6a-2).

### Câu hỏi 2 — Ứng dụng bật transceiver **trước** `HAL_CAN_Start`

Sửa một rủi ro của GĐ 4 (mục 2.6): bật sau `Start` có thể làm module phát **cờ lỗi giữa frame của module khác** trên bus đo. **Đề xuất: duyệt.**

### Câu hỏi 3 — Hai linker script, bạn sửa tay

- `VF_TempHumi_Bootloader/STM32F303K8TX_FLASH.ld`: `LENGTH = 12K` (IOC Guide B4) — làm lúc tạo project.
- `STM32F303K8TX_FLASH.ld` của ứng dụng: `ORIGIN = 0x8003000` (giữ `LENGTH = 50K`) — làm **khi tôi báo**, cùng lần nạp bootloader đầu tiên.

`check_linker` kiểm cả hai bằng cùng hằng số. **Đề xuất: duyệt.**

### Câu hỏi 4 — Mã trả cho `SET_FULL_SCALE` (`0x13`) và `SET_GYRO_FS` (`0x14`)

CAN2 §4.2 ghi "không hỗ trợ" mà không nói mã nào. Theo định nghĩa §4 (`0x01` = "loại module này không có lệnh đó", ví dụ chính là `0x14` gửi tới module rung) — **đề xuất `0x01`**. `SET_TX_PERIOD` (`0x12`) thì hợp đồng ghi rõ `0x02`. Mainboard không gửi hai lệnh đó cho loại `0x3` (không có nút), nên điều này chỉ hiện ra khi thử bằng analyzer.

### Câu hỏi 5 — Dây nối J2 ↔ module

J2 của mainboard là 4 chân (`+5V`, `CAN2_H`, `GND`, `CAN2_L` — CAN2 §1); module dùng JP3 8 chân (`+5` 1/2, `CAN_H` 3/4, `CAN_L` 5/6, `GND` 7/8). **Bạn có sẵn dây chuyển đổi chưa?** Nếu chưa, bài 6 dời lại; bài 1–5 chạy bằng analyzer đóng vai mainboard. Lưu ý: cấu hình chỉ cần 5 V từ J2 — không cần +12 V cho cảm biến.
