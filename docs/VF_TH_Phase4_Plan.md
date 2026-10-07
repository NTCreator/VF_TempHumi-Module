# Giai đoạn 4 — Kế hoạch: kho config trong flash, và lần phát thật đầu tiên

**Trạng thái: CHỜ DUYỆT** (2026-10-07). Chưa viết dòng code nào. Ba câu hỏi ở mục 11.

Căn cứ: CAN2 Protocol v1.5 §4, §4.1, §4.2, §5 (bất biến 2, 3, 5); ID Map v1.3 §2.1, §2.3; DBC v1.5; kết quả GĐ 3; cách module góc làm giai đoạn 6a-1 (`VF_Angle_Module/docs/VF_Angle_Phase6a1_Plan.md`) — chép cách làm, không chép code.

---

## 0. Mục tiêu — kiểm được trên board

1. **Config sống trong flash**, page cuối **`0x0800F800`** (2 KB), có magic + version + CRC-32. `node_id` đi ra **từ đó**. **`APP_CAN_NODE_ID_PHASE3` bị xoá.**
2. **Config không hợp lệ — kể cả page trắng — ⇒ IM LẶNG trên CAN1**, ba lớp độc lập: silent loopback, transceiver standby, cổng `node_id`. Chứng minh trên **một page trắng thật**, với analyzer nghe trên bus.
3. **Config hợp lệ ⇒ lần phát thật đầu tiên**: `TH_Data_01` / `TH_Status_01` mỗi 1 s trên dây, giải mã bằng DBC v1.5 ở analyzer.
4. **Mainboard ghi được vào `.blf`, giải mã bằng DBC v1.5** — tiêu chí lộ trình của giai đoạn này.
5. **Không bao giờ có frame cũ lên dây**, kể cả khi bus mất ACK rồi có lại.

---

## 1. Dữ kiện đã kiểm

| Kiểm | Kết quả | Hệ quả |
|---|---|---|
| **Page flash F303x8** | `FLASH_PAGE_SIZE = 0x800` (`stm32f3xx_hal_flash_ex.h:196`, định nghĩa chung cho mọi F3) — **2 KB**, không phải 1 KB như F103 của module góc. `IS_FLASH_NB_PAGES` cho khối `STM32F303x8` giới hạn ở `0x0800FFFF` khi flash 64 KB | Page cuối của 64 KB là **`0x0800F800`** |
| **Bản đồ flash — sửa một con số của GĐ 1** | `app_config.h` ghi vùng ứng dụng **52 KB** (= 64 − 12 bootloader). **Sai: chưa trừ page config.** 12 + **50** + 2 = 64 | `APP_APP_FLASH_KB` → **50**. GĐ 3 dùng 26,5 KB = **53%** (không phải 49,8%) |
| **Linker** | `STM32F303K8TX_FLASH.ld`: `FLASH ... LENGTH = 64K` — ứng dụng **về nguyên tắc với tới page config**. Hiện 26,5 KB nên chưa chạm, nhưng không có gì chặn | câu hỏi 1 |
| **`AutoRetransmission = DISABLE`** trong `can.c` (từ `.ioc`) | NART = 1: frame **không bao giờ được phát lại** — mất ACK hay **thua arbitration** là mất luôn. `0x400` ưu tiên **thấp hơn** mọi `0x2xx` (rung) và `0x3xx` (góc) | **Trên bus đông, `TH_Data` sẽ mất thường xuyên.** `app_can_init` (đằng nào cũng gọi lại `HAL_CAN_Init`) đặt **`AutoRetransmission = ENABLE`** khi lên dây — không sửa `.ioc`. Frame cũ chặn bằng 2.4 |
| **`AutoBusOff = DISABLE`** | sau bus-off, bxCAN **nằm im tới khi reset** | đặt **`AutoBusOff = ENABLE`** cùng chỗ: tự hồi sau 128 × 11 bit lặn; `BUS_OFF_SEEN` (đã có từ GĐ 3) giữ dấu |
| HAL flash | `HAL_FLASH_MODULE_ENABLED`; `_flash.c`, `_flash_ex.c` có trong `Drivers/`; ghi theo **half-word** (`FLASH_TYPEPROGRAM_HALFWORD`) | 16 byte = 8 half-word |
| Hợp đồng | CAN2 §4.2: loại `0x03` **không có** `SET_TX_PERIOD` hợp lệ, không `SET_FULL_SCALE`, không `SET_GYRO_FS` ⇒ config chỉ cần `node_id` + `module_type` | struct nhỏ hơn module góc (mục 2.1) |
| **`VF_Main` biết loại `0x3`** | commit `86aec85` "hợp đồng v1.5: thêm module nhiệt-ẩm"; `APP_MODCFG_TYPES_IMPLEMENTED` có bit 3; nút "TempHumi" gọi `app_guidata_cfg_type(3)` — **đúng** | mainboard ghi và nhận diện được module này |
| *(ngoài repo)* Hai comment cũ trong `VF_Main` | `app_guidata.h:249` ghi "3 TempRTD, 4 TempHumi" (ngược); `ConfigView.cpp:160` ghi TempHumi "chưa có firmware". **Code đúng, chỉ comment sai** | dọn ở phiên `VF_Main` |

---

## 2. Thiết kế

### 2.1 Struct config — 16 byte, kích thước đó là định dạng trên flash

```c
typedef struct {
  uint32_t magic;          /* "TCF1" = 0x31464354 - khác "VCF1" (rung), "ACF1" (góc) */
  uint16_t version;        /* 1 */
  uint8_t  node_id;        /* 0x30..0x39; 0xFF = chưa cấu hình */
  uint8_t  module_type;    /* phải là 0x03 */
  uint8_t  reserved[4];    /* 0 */
  uint32_t crc32;          /* CRC-32 IEEE phản chiếu, trên 12 byte đầu */
} app_cfg_t;
```

- **Không có trường chu kỳ**: loại `0x3` cố định 1000 ms (CAN2 §4.2, phương án A). Một trường không ai đặt được là một chỗ cho giá trị sai.
- Cùng kích thước 16 byte với module góc; magic riêng ⇒ page do firmware loại khác ghi bị từ chối ngay bước đầu, và `g_diag` nói vì sao.
- `_Static_assert` `sizeof` = 16 và từng `offsetof`. Chỉ kiểu số nguyên cố định (enum ở đây 1 byte).
- **`app_cfg` thuần**, không HAL: bootloader (GĐ 5) sẽ **link đúng file này** — CAN2 §5 bất biến 5: "phép kiểm config mà bootloader dùng phải là **đúng cái hàm** ứng dụng dùng".
- **CRC-32 chuẩn** (`0xEDB88320`, từng bit), không dùng ngoại vi CRC của STM32 — để PC và bootloader kiểm được. Test bằng `"123456789"` → `0xCBF43926`.

**Hợp lệ khi đủ cả bảy, theo thứ tự này** — `app_cfg_check()` trả **lý do đầu tiên**:

| # | Kiểm | Lý do |
|---|---|---|
| 1 | magic ≠ `0xFFFFFFFF` | **trắng** — chưa bao giờ cấu hình |
| 2 | magic = `"TCF1"` | magic sai — hỏng, hoặc page của loại khác |
| 3 | version = 1 | layout khác |
| 4 | CRC | hỏng / mất điện giữa lúc ghi |
| 5 | `module_type` = `0x03` | sai loại — **kiểm thật**, lớp thứ hai sau quy tắc nibble |
| 6 | `APP_NODE_ID_OK(node_id)` | ngoài `0x30`–`0x39`, gồm `0xFF` |
| 7 | `reserved` = 0 | layout lạ dưới cùng version |

"Trắng" tách khỏi "magic sai": **chưa cấu hình** và **đã hỏng** đều im lặng, nhưng người đọc `g_diag` phân biệt được.

### 2.2 `app_cfg_store` — đọc, ghi, khởi động

| Hàm | Việc |
|---|---|
| `app_cfg_store_read()` | `memcpy` từ `0x0800F800`. Page trắng đọc ra `0xFF` ⇒ lý do 1 |
| `app_cfg_store_write()` | `seal` (magic, version, CRC) → unlock → xoá page → ghi 8 half-word → lock → **đọc lại và `memcmp`**. Mọi `HAL_*` kiểm status |
| `app_cfg_boot()` | **một lần** từ `main()`, **trước** `app_th_init` (xoá page làm CPU đứng — chưa có giao dịch Modbus nào bay lúc đó). Đọc, validate, điền `g_diag.cfg` |
| `app_cfg_active()` | con trỏ tới config đã validate, hoặc **`NULL`**. Không có đường nào đi từ `NULL` ra một `node_id` |

Giữa lúc xoá xong và lúc ghi xong, page mang một config **trượt CRC của chính nó** ⇒ mất điện đúng lúc đó = chưa cấu hình = im lặng (CAN2 §5 bất biến 3).

**Ai ghi config ở GĐ 4?** Chưa có bootloader (GĐ 5), nên chỉ **công tắc bench**:

| Công tắc (`app_config.h`) | Mặc định | Việc |
|---|---|---|
| `APP_CFG_BENCH_WRITE` | **0** | config hiện tại không hợp lệ ⇒ ghi config bench (`node_id` `0x30`) lúc khởi động |
| `APP_CFG_BENCH_ERASE` | **0** | xoá page lúc khởi động ⇒ quay về page trắng |
| `APP_BENCH_CAN_LOOPBACK` | **0** | config hợp lệ vẫn silent loopback + standby — thử trên bàn không có bus. **Chỉ làm module im hơn, không bao giờ ồn hơn** |

Bật bất kỳ công tắc nào ⇒ `#warning` lúc build + cờ trong `g_diag` + tag trong `s_fw_id`. **Bài học module rung:** page trắng chỉ dùng được **một lần** — nên `WRITE` mặc định 0, và **lần nạp đầu tiên chính là bài page trắng**.

`g_diag.cfg.node_id_source`: `0` không có · `2` **CONFIG** (có sẵn, build không ghi) · `3` **BENCH** (build này vừa ghi). Giá trị `1` (PHASE3) bỏ, không tái dùng.

### 2.3 Hai trạng thái của CAN — quyết định ở **đúng một chỗ**

`app_can_init()` hỏi `app_cfg_active()`:

| | Config hợp lệ | Config **không** hợp lệ (kể cả trắng) |
|---|---|---|
| `node_id` | từ config | `0xFF` |
| bxCAN | **NORMAL**, `AutoRetransmission` + `AutoBusOff` **ENABLE** | silent loopback (như GĐ 3) |
| Transceiver | **Normal** (PA10 LOW) | **Standby** (PA10 HIGH) |
| Filter | **danh sách: đúng một ID `0x010 + node_id`** (`0x040`) | mở, như GĐ 3 |
| Cổng | mở | chặn mọi frame, `gate_blocked++` |
| LED | như GĐ 2 | **mã 3** `APP_FAULT_CFG_CRC` — câu hỏi 2 |

**Hàm đưa transceiver sang Normal đòi config làm bằng chứng** — lần đầu tiên nó tồn tại:

```c
bool app_board_can_transceiver_normal(const app_cfg_t *proof);   /* tự gọi app_cfg_valid() */
```

Không hợp lệ ⇒ giữ Standby, trả `false`. "Chỉ config hợp lệ gọi được" thành **tính chất của hàm**, không phải lời hứa của người gọi. Gọi **sau** `HAL_CAN_Start()` thành công, không bao giờ trước.

### 2.4 Trên dây thật: không bao giờ phát frame cũ

Với `AutoRetransmission` bật: **không ai ACK** (analyzer listen-only, chưa cắm, bus chỉ có module này) ⇒ phát lại vô hạn, mailbox đầy; khi ACK quay lại, frame **cũ** đi trước frame mới. Thua arbitration liên tục cũng vậy.

Luật: **mỗi loại frame có tối đa một frame chờ trong mailbox.** Trước khi xếp frame mới, frame trước **của đúng loại đó** còn chờ (`HAL_CAN_IsTxMessagePending`) ⇒ huỷ (`HAL_CAN_AbortTxRequest`), `tx_aborted++`.

- Không frame nào lên dây trễ quá **một chu kỳ** (1 s).
- `Counter` **nhảy** đúng nghĩa đã chốt ở GĐ 3: một frame tới hạn mà không tới log.

Ở NORMAL không có vọng về để so; thước đo là ESR mỗi giây (TEC, REC, LEC, EWG/EPV/BOFF), `tx_aborted`, `tx_full`, và **analyzer**.

### 2.5 Filter

Ở NORMAL: bank 0 **danh sách**, đúng `0x040` (`TH_Cmd_01`, ID Map §2.3). Lệnh là GĐ 6: chỉ đọc ra, `rx_cmd_seen++`, bỏ — đọc để FIFO không đầy, đếm để bài 7 chứng minh filter đúng. Ở loopback: mở, so byte như GĐ 3.

---

## 3. File

| File | | Việc |
|---|---|---|
| `Core/{Inc,Src}/app/app_cfg.*` | **mới, thuần** | struct, CRC-32, `seal`, `defaults`, `check`, `valid`, `*_ok()` từng trường (GĐ 5 dùng lại cho `SET_*`) |
| `Core/{Inc,Src}/app/app_cfg_store.*` | **mới** | đọc / ghi / xoá page, `app_cfg_boot`, `app_cfg_active`, công tắc bench |
| `app_board.*` | sửa | `app_board_can_transceiver_normal(const app_cfg_t *proof)` |
| `app_can.*` | sửa | hai trạng thái; NORMAL + AutoRetx + AutoBusOff; huỷ frame cũ; filter danh sách; `node_id` từ config; `tx_aborted`, `rx_cmd_seen`, `transceiver_on` |
| `app_config.h` | sửa | **xoá `APP_CAN_NODE_ID_PHASE3`**; `APP_APP_FLASH_KB` 52 → **50**; `APP_CFG_FLASH_ADDR`, `APP_CFG_PAGE_KB`, magic, version; ba công tắc bench; `APP_NODE_ID_BENCH` `0x30`; `APP_FW_PHASE "4"`; `_Static_assert` 12 + 50 + 2 = 64 |
| `app_fault.h` | sửa | **`APP_FAULT_CFG_CRC = 3`** (số đã giữ chỗ từ GĐ 1) |
| `app_diag.h` | sửa | `g_diag.cfg` |
| `main.c` | sửa, USER CODE | `app_cfg_boot()` sau `app_led_init`, trước `app_th_init` |
| `tools/check_linker.sh`, `makefile.init` | **mới / sửa** | câu hỏi 1 |
| `tests/test_app_cfg.c`, `Makefile` | mới / sửa | mục 6 |
| `STM32F303K8TX_FLASH.ld` | **bạn sửa tay** | câu hỏi 1 |

**Hai file `.c` mới ⇒ Refresh + Build Project trong IDE một lần.**

## 4. API

```c
/* app_cfg.h - pure, linked by the bootloader in phase 5 */
#define APP_CFG_MAGIC    0x31464354u   /* "TCF1" */
#define APP_CFG_VERSION  1u
typedef enum { APP_CFG_OK = 0, APP_CFG_BLANK, APP_CFG_BAD_MAGIC, APP_CFG_BAD_VERSION,
               APP_CFG_BAD_CRC, APP_CFG_BAD_TYPE, APP_CFG_BAD_NODE, APP_CFG_BAD_RESERVED } app_cfg_state_t;
uint32_t app_cfg_crc32(const uint8_t *p, uint32_t n);
void     app_cfg_defaults(app_cfg_t *c);       /* node 0xFF: unconfigured = silent */
void     app_cfg_seal(app_cfg_t *c);
uint8_t  app_cfg_check(const app_cfg_t *c);    /* app_cfg_state_t */
bool     app_cfg_valid(const app_cfg_t *c);
bool     app_cfg_node_id_ok(uint8_t id);       /* = APP_NODE_ID_OK */
bool     app_cfg_module_type_ok(uint8_t t);

/* app_cfg_store.h */
void             app_cfg_store_read(app_cfg_t *out);
bool             app_cfg_store_write(app_cfg_t *c);   /* seals, erases, programs, reads back */
bool             app_cfg_store_erase(void);           /* bench only */
void             app_cfg_boot(void);
const app_cfg_t *app_cfg_active(void);                /* NULL = silent */
```

## 5. Ngân sách

| | Ước tính |
|---|---|
| Flash | **+2–3 KB** ở `-O0` (HAL flash erase/program + `app_cfg` + store) ⇒ ~29 KB = **~58% của 50 KB** |
| RAM | ~60 B (`g_diag.cfg`, bản sao config) |
| Tải bus | 2 frame / s ≈ **0,04%** — lần đầu tiên đo được, bằng analyzer |

## 6. Test trên PC — `tests/test_app_cfg.c`

| Test | Chứng minh |
|---|---|
| CRC `"123456789"` = `0xCBF43926` | CRC-32 IEEE chuẩn |
| 16 × `0xFF` ⇒ lý do 1; 16 × `0x00` ⇒ lý do 2 | trắng và hỏng đều im lặng, phân biệt được |
| **lật từng bit trong 16 byte** của config hợp lệ (128 lần) ⇒ không lần nào hợp lệ | CRC bắt mọi lỗi 1 bit |
| version 0 / 2 ⇒ 3 · `module_type` 1 / 2 / 4 (đã seal lại) ⇒ 5 · `reserved` ≠ 0 (seal lại) ⇒ 7 | từng phép kiểm thật |
| `node_id`: quét 256 giá trị (seal lại mỗi lần) ⇒ **đúng 10** hợp lệ (`0x30`–`0x39`) | `APP_NODE_ID_OK` là nguồn duy nhất |
| `defaults()` ⇒ CRC đúng nhưng `node_id` `0xFF` ⇒ lý do 6 | mặc định xuất xưởng là im lặng |
| `offsetof` từng trường, `sizeof` = 16 | định dạng trên flash khoá cứng |
| config bench so với byte viết tay `54 43 46 31 01 00 30 03 00 00 00 00` + CRC | little-endian đúng như 2.1 |

## 7. Test trên board (phác thảo — bản đầy đủ sau khi code)

Cần: analyzer ở chế độ **thường (có ACK)** + DBC v1.5, 120 Ω; **bus cô lập** cho bài 1–8; **mainboard** cho bài 9.

| # | Làm gì | Mong đợi |
|---|---|---|
| **1** | **ĐẦU TIÊN.** Build mặc định (mọi công tắc 0). Analyzer Start **trước**, rồi cấp nguồn module, rút–cắm 3 lần | `cfg.state` = 1 (**trắng**), `node_id` 255, loopback, `transceiver_on` 0, **PA10 ≈ 3,3 V**, **LED mã 3**. Analyzer: **không frame nào, không error frame nào**. *(`state` ≠ 1 ⇒ page không trắng: build `ERASE = 1` một lần rồi làm lại)* |
| 2 | `WRITE = 1` | `state` 0, `node_id_source` 3, NORMAL, **PA10 ≈ 0 V**. Analyzer + DBC v1.5: **`TH_Data_01` và `TH_Status_01` mỗi 1 s**: `Temperature` ≈ nhiệt độ phòng, `Humidity` hợp lý, `DataAge` 0…2 s, `Counter` +1 / s, `NodeId` 0x30, `TxPeriod` 1000 ms. TEC = 0 |
| 3 | Reset (không nạp lại) | `state` 0 ngay, **`writes` 0**: config sống qua reset, không ghi lại |
| 4 | Rút J1 15 s, xem đồ thị `DataAge` ở analyzer | `DataAge` leo đều, `Flags` bật STALE sau 6 s; `Counter` vẫn liền — **lần đầu tiên thấy được trên dây thật** |
| 5 | **Mất ACK**: dừng analyzer 10 s (nó là node duy nhất khác), rồi chạy lại | lúc mất: TEC leo, error-passive, `tx_aborted` tăng. Có lại: frame đầu có `Counter` **nhảy**, **không** frame cũ nào đi trước |
| 6 | `WRITE = 0` (đường sản phẩm) | `node_id_source` 2, vẫn phát như bài 2 |
| 7 | **Filter**: analyzer gửi `0x040`, rồi `0x041` | `rx_cmd_seen` +1 cho `0x040`, **không** cho `0x041`. **Không bao giờ gửi `0x400` / `0x402` từ analyzer** — đó là ID của chính module |
| 8 | Tải bus ở analyzer | ≈ 0,04% |
| **9** | **Mainboard**: bus = `VF_Main` + module này (**không** module TH nào khác). `VF_Main` ghi `.blf` 2 phút | mở `.blf` bằng DBC v1.5: `TH_Data_01` 1 Hz, số đúng; `TH_Status_01` có; mainboard nhận diện `TEMPHUMI` node `0x30` |
| 10 | Hồi quy GĐ 2: rút/cắm cảm biến, LED, `g_diag.th` | như GĐ 2 |
| 11 | *(tuỳ chọn)* `APP_BENCH_CAN_LOOPBACK = 1` với config hợp lệ | loopback, PA10 3,3 V, analyzer không thấy gì |

**Xong thì trả mọi công tắc về 0.** Board giữ config bench trong flash; với `WRITE = 0` nó chạy đường của sản phẩm.

---

## 8. Rủi ro

| Rủi ro | Cách chặn |
|---|---|
| Lần chạy đầu ghi mất page trắng | `WRITE` mặc định 0; bài 1 là lần nạp đầu; `ERASE` để quay lại |
| Phát khi config hỏng | ba lớp (2.3); hàm transceiver đòi bằng chứng; 128 bit lật trên host; bài 1 trên bus thật |
| **Trùng ID** trên bus thật | bài 1–8 bus cô lập; bài 9 chỉ mainboard + module này |
| Frame cũ ra dây sau khi mất ACK | 2.4; bài 5 |
| Frame `TH` mất vì thua arbitration (NART) | `AutoRetransmission = ENABLE` khi NORMAL (mục 1) |
| Flash mòn vì ghi mỗi lần khởi động | chỉ ghi khi config **không** hợp lệ và có công tắc; `g_diag.cfg.writes`; bài 3 |
| **Ứng dụng tràn vào page config** | câu hỏi 1 |
| Layout lệch giữa ứng dụng và bootloader (GĐ 5) | một `app_cfg.c` dùng chung; `offsetof` khoá cứng |

## 9. Cố tình KHÔNG làm

| | Vì sao |
|---|---|
| Không giao thức CAN2, không bootloader | GĐ 5. Config chỉ ghi được bằng công tắc bench |
| Không xử lý `TH_Cmd` | GĐ 6. Filter đã đúng ID, chỉ đếm |
| Không dời `ORIGIN` ứng dụng sang `0x08003000` | GĐ 5, cùng bootloader. Chỉ `LENGTH` mới cần ngay (câu hỏi 1) |

## 10. Ghi lại cho GĐ 5

Bootloader **link `app_cfg.c`** (không chép). `SET_NODE_ID` / `SET_MODULE_TYPE` dùng `*_ok()`; `SET_TX_PERIOD` **luôn** `0x02`; `SET_FULL_SCALE`, `SET_GYRO_FS` **luôn** `0x02` (CAN2 §4.2). `COMMIT` gọi đúng `app_cfg_store_write`. Bootloader ở lại khi config không hợp lệ (CAN2 §5 bất biến 5, v1.4).

---

## 11. Cần bạn quyết

### Câu hỏi 1 — Chặn ứng dụng tràn vào page config

Linker đang `LENGTH = 64K`. Page config là 2 KB cuối. Hiện ứng dụng 26,5 KB, chưa chạm — nhưng không có gì **chặn**.

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | **Bạn sửa tay** `STM32F303K8TX_FLASH.ld`: `FLASH ... LENGTH = 64K` → **`50K`** (`ORIGIN` giữ `0x8000000` tới GĐ 5) — như module góc (IOC Guide B3 bên đó). **Và tôi thêm `tools/check_linker.sh` vào `makefile.init`**: build fail nếu vùng FLASH của linker với tới `0x0800F800`, gọi tên file | Tràn = lỗi link, không phải module hỏng ngoài hiện trường. File `.ld` do CubeMX sinh và **có thể bị Generate ghi đè** — `check_linker` biến chuyện đó từ im lặng thành build đỏ, như `check_contracts` làm với hợp đồng |
| B | Không sửa linker; chỉ báo kích thước mỗi giai đoạn | Không chặn gì; dựa vào người đọc báo cáo |

### Câu hỏi 2 — Config không hợp lệ thì LED nói gì?

| | Phương án | Hệ quả |
|---|---|---|
| **A** ✅ đề xuất | **Mã 3 `APP_FAULT_CFG_CRC`, chốt, không fatal** — kể cả page trắng. Cảm biến vẫn đo (để chẩn đoán qua `g_diag`). Mã 3 thắng mã 2 (sống) | Lúc đó module **im lặng trên CAN1** — LED là tiếng nói duy nhất. Giống module góc. Số 3 đã giữ chỗ từ GĐ 1 |
| B | Page trắng thì không báo; chỉ báo khi hỏng | Một module chưa cấu hình trông y hệt module đang chạy |

### Câu hỏi 3 — Mainboard cho bài 9

Tiêu chí của giai đoạn này là **mainboard ghi được `.blf`**. Cần một mainboard chạy `VF_Main` **từ commit `86aec85` trở đi** (có loại `0x3`).

- Bạn có mainboard với bản đó không, hay cần nạp?
- Nếu chưa có ⇒ giai đoạn 4 đóng bằng bài 1–8 (analyzer + DBC v1.5), bài 9 dời sang GĐ 6 (tích hợp).
