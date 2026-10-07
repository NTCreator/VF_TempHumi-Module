# Giai đoạn 4 — Báo cáo

**Trạng thái: XONG 2026-10-07** — build sạch, test host 2 497/2 497, test board đạt cả 10 bài bắt buộc, **mainboard ghi `.blf` và giải mã được bằng DBC v1.5** (tiêu chí lộ trình). **Một phép đo cần đo lại** (PA10 ở bài 2, mục 6.1) — không chặn, lý do ở đó.

Config 16 byte sống ở page cuối `0x0800F800`; `node_id` đi ra từ đó và từ đâu khác. Không có config hợp lệ ⇒ im lặng trên CAN1 (chứng minh trên một page trắng thật). Có ⇒ **module phát lần đầu tiên lên dây thật**.

## 1. Kế hoạch đã duyệt

`docs/VF_TH_Phase4_Plan.md`, duyệt 2026-10-07:

| Câu hỏi | Quyết định |
|---|---|
| 1. Chặn ứng dụng tràn vào page config | **Người dùng sửa tay** `STM32F303K8TX_FLASH.ld`: `FLASH LENGTH 64K → 50K`. Thêm `tools/check_linker.sh` vào `makefile.init` |
| 2. LED khi config không hợp lệ | **Mã 3** `APP_FAULT_CFG_CRC`, chốt, không fatal, kể cả page trắng |
| 3. Mainboard cho bài `.blf` | `VF_Main` đang chạy **`41ad090`**. Đã kiểm trong repo `VF_Main`: `86aec85` (loại `0x3`) là tổ tiên của nó; filter CAN1 nhận **mọi ID chuẩn** (chỉ tách theo bit 0 vào hai FIFO); `app_modreg` nhận diện dải `0x100`–`0x5FF`, đọc **`Counter` ở byte 6 và `Flags` ở byte 7 của `TH_Data`** với đúng bit `0x01`…`0x20`, và có sẵn trạng thái `NO SENSOR` / `STALE` / `TIMEOUT` cho loại này ⇒ **không cần nạp lại, bài 9 làm ngay** |

Không có chỗ làm khác kế hoạch.

## 2. Hai sửa sai phát hiện khi lập kế hoạch

1. **Vùng ứng dụng là 50 KB, không phải 52 KB.** GĐ 1 viết `APP_APP_FLASH_KB = 64 − 12` và quên page config. Page trên F3 là **2 KB** (`FLASH_PAGE_SIZE 0x800`, định nghĩa chung cho mọi F3): 12 + **50** + 2 = 64. Con số "49,8%" của báo cáo GĐ 3 đúng ra là **53%**.
2. **`AutoRetransmission = DISABLE`** và **`AutoBusOff = DISABLE`** trong `can.c` (sinh từ `.ioc`). Vô hại ở loopback, nhưng trên bus thật `0x400` thua arbitration trước mọi `0x2xx` / `0x3xx` của module rung và góc — mỗi lần thua là **mất frame vĩnh viễn**; và một lần bus-off là **im tới khi reset**. `app_can_init` (vốn đã gọi lại `HAL_CAN_Init`) bật cả hai khi lên dây — không sửa `.ioc`. Frame cũ chặn bằng huỷ trong mailbox (mục 3).

## 3. File đã tạo / sửa

| File | Việc |
|---|---|
| `Core/{Inc,Src}/app/app_cfg.*` | **Mới, thuần** — bootloader GĐ 5 sẽ link chính file này. Struct 16 byte `"TCF1"` (`_Static_assert` size + từng `offsetof`), CRC-32 IEEE từng bit, `seal`, `defaults` (node `0xFF` ⇒ im lặng), `check` 7 bước trả lý do đầu tiên, `*_ok()` cho CAN2 `SET_*` |
| `Core/{Inc,Src}/app/app_cfg_store.*` | **Mới.** Đọc (`memcpy`), xoá, ghi 8 half-word, **đọc lại và so**; mọi đường đi qua `HAL_FLASH_Lock`. `app_cfg_boot()` + ba công tắc bench, `app_cfg_active()` (`NULL` ⇒ không có `node_id`). Công tắc bật ⇒ đúng một `#warning` ở file này |
| `app_board.*` | **`app_board_can_transceiver_normal(const app_cfg_t *proof)`** — tự gọi `app_cfg_valid`, hỏng ⇒ standby; `_standby()`, `_stb_level()` |
| `app_can.*` | Hai trạng thái quyết định ở một chỗ; NORMAL + AutoRetx + AutoBusOff; filter **danh sách** `0x010 + node_id`; transceiver Normal **sau** `HAL_CAN_Start`; **tối đa một frame chờ mỗi loại** — còn chờ ⇒ `AbortTxRequest`, `tx_aborted++`; `rx_cmd_seen`. Hỏng ở bất kỳ bước nào ⇒ transceiver standby |
| `app_config.h` | **Xoá `APP_CAN_NODE_ID_PHASE3`**; bản đồ flash 12 + 50 + 2; `APP_CFG_FLASH_ADDR`; ba công tắc bench (mặc định 0, WRITE + ERASE ⇒ `#error`); tag `s_fw_id`; `APP_FW_PHASE "4"` |
| `app_fault.h` | **`APP_FAULT_CFG_CRC = 3`** |
| `app_diag.h`, `main.c` | `g_diag.cfg`; `app_cfg_boot()` sau `app_led_init`, trước `app_th_init` / `app_can_init` (chỉ USER CODE) |
| `tools/check_linker.sh`, `makefile.init` | **Mới.** Vùng FLASH của linker với tới `APP_CFG_FLASH_ADDR` ⇒ build đỏ, gọi tên file |
| `STM32F303K8TX_FLASH.ld` | **Người dùng sửa:** `LENGTH = 50K` |
| `tests/test_app_cfg.c`, `Makefile` | Mục 4 |
| `docs/VF_TH_Phase4_Test.md` | Quy trình test |

## 4. Test trên host — 2 497 / 2 497

`test_app_cfg` (920 kiểm tra): CRC `"123456789"` = `0xCBF43926`; 16 × `0xFF` ⇒ **trắng**, 16 × `0x00` ⇒ **magic sai** (phân biệt được); `defaults()` có CRC đúng nhưng **không hợp lệ** (node `0xFF`); **lật từng bit trong 128 bit** ⇒ không lần nào hợp lệ; version 0 / 2; CRC lệch 1 bit; `module_type` 0, 1, 2, 4, 0xFF (đã seal lại) ⇒ loại; từng byte `reserved` ≠ 0 (seal lại) ⇒ reserved; magic `"ACF1"` / `"VCF1"` ⇒ magic sai; **quét 256 `node_id` ⇒ đúng 10 hợp lệ**; byte viết tay `54 43 46 31 01 00 30 03 00 00 00 00` + CRC `0x2E9C0667`.

**6 / 6 đột biến bị bắt**: bỏ kiểm loại, gộp trắng vào magic sai, bỏ kiểm reserved, CRC trên 8 byte thay vì 12, sai đa thức, kiểm node lệch một.

## 5. Build, bộ nhớ, và hai chốt chặn trong build

**Build trong IDE** (người dùng, 16:18:11): `0 errors, 0 warnings`, và log mở đầu bằng **`check_contracts: ok`** rồi **`check_linker: ok, FLASH ends at 0x0800C800, config page at 0x0800F800`**. **Rebuild sạch headless**: 0 / 0, cùng con số. Mọi công tắc bench = 0 trong commit.

```
   text    data     bss     dec     hex  filename
  28052      12    2244   30308    7664  VF_TempHumi_Module.elf      (Debug, -O0)
```

| | GĐ 3 | GĐ 4 | Có | Tỷ lệ |
|---|---|---|---|---|
| Flash | 26 500 | **28 064** | **51 200 (50 KB)** | **54,8%** |
| RAM tĩnh | 648 | **720** | 12 288 | 5,9% |

+1,5 KB (trong ước lượng +2–3 KB): `app_cfg` 544 B, `app_cfg_store` 736 B, `app_can` +440 B; HAL flash erase/program. Stack đường ghi (`app_cfg_store_write` 72 B + HAL flash) chỉ chạy lúc khởi động, nông hơn đường quét GĐ 2.

**`check_linker` đã chứng minh fail được:** đổi `.ld` sang `LENGTH = 64K`, `make` với PATH chỉ có tool của ST:

```
check_contracts: ok, 4 files match VF_Main.
check_linker: FAILED - the application may reach the config page.

  STM32F303K8TX_FLASH.ld: FLASH ORIGIN 0x8000000 LENGTH 64K ends at 0x08010000
  config page (APP_CFG_FLASH_ADDR) starts at   0x0800F800

  A CubeMX Generate may have rewritten the linker script. ...
../makefile.init:43: *** the linker script lets the application reach the config page - see the report above.  Stop.
make exit code: 2
```

`make clean` trong lúc sai: exit 0. Hoàn nguyên, `cmp` giống hệt. Chạy được với cả `sh` của Git Bash lẫn BusyBox của ST.

**Công tắc bench:** bật từng cái ⇒ **đúng một** `#warning` trên toàn bộ nguồn ứng dụng, và đúng tag (`BENCH_WRITE` / `BENCH_ERASE` / `BENCH_LOOPBACK`) trong `s_fw_id`.

## 6. Kết quả test trên board — 2026-10-07

| # | Bài | Kết quả |
|---|---|---|
| 1 | **Page trắng** — lần nạp đầu | **PASS** — LED **mã 3**; `g_diag.th.count[0]` vẫn tăng (52, mọi lỗi 0) — cảm biến vẫn đo; **PA10 = 3,3 V**; **analyzer không một frame**. `cfg`: `state` = `state_at_boot` = **1 (trắng)**, `node_id` 255, `source` 0, công tắc 0/0/0, `write_status` = `erase_status` = 255, `module_type` 255, `version` 65535, `magic` = `crc_stored` = 4294967295, `crc_calc` **3147431818**, `writes` 0 |
| 2 | **`WRITE = 1` — lần phát thật đầu tiên** | **PASS** — **analyzer thấy frame**, `NodeId` 48, `TxPeriod` 100 (×10 ms), `ModbusOk` tăng, `ModbusErr` 0; thở / nắm cảm biến ⇒ T và RH đổi trên dây; LED 1 nháy / 5 s. `cfg`: `state_at_boot` 1 → **`state` 0**, `node_id` 48, `source` **3**, `bench_write` 1, `write_status` 0, `module_type` 3, `version` 1, **`magic` 826688340** ("TCF1"), **`crc_stored` = `crc_calc` = 781977191**, `writes` 1. `can`: `mode` **2**, `transceiver_on` **1**, **`stb_level` 0**, **`slots` = `data_sent` = `status_sent` = 206**, `counter` 205, `tx_aborted` = `tx_full` = `tx_errors` = 0, TEC = REC = LEC = ESR = 0, `bus_off_seen` 0, ID 1024 / 1026. **Người dùng báo thêm: PA10 = 3,3 V** — mục 6.1 |
| 3 | Reset | **PASS** — `state_at_boot` 0, **`writes` 0**: config sống qua reset, không ghi lại |
| 4 | Rút J1 15 s | **PASS** — `DataAge` leo ~1 s mỗi giây, `Flags` 2 rồi **10** sau 6,1 s, `Counter` liền; cắm lại ⇒ `DataAge` ≤ 2 s, `Flags` 0. **Lần đầu tiên tuổi số đo được thấy trên dây thật** |
| 5 | Mất ACK 10 s | **PASS** |
| 6 | `WRITE = 0` — đường sản phẩm | **PASS** |
| 7 | Filter | **PASS** — `rx_cmd_seen` +1 cho `0x040`; `rx_frames` **không đổi** cho `0x041` (filter phần cứng chặn) |
| 8 | Tải bus | **PASS** |
| **9** | **Mainboard `VF_Main` `41ad090`, ghi `.blf`** | **PASS** — tiêu chí lộ trình của GĐ 4 đạt |
| 10 | Hồi quy GĐ 2–3 | **PASS** |

**CRC trên chip = CRC trên PC, từng số:** page trắng `3147431818` = `0xBB99FF8A` = `app_cfg_crc32(12 × 0xFF)` tính trên host; config bench `781977191` = `0x2E9C0667` = byte viết tay trong `test_app_cfg`. Đó là điều GĐ 5 cần: bootloader và một công cụ PC sẽ tính ra **cùng** con số.

### 6.1 PA10 ở bài 2 — số đo mâu thuẫn với hai bằng chứng độc lập; cần đo lại

Bài 2 mong đợi PA10 ≈ 0 V (transceiver Normal). Người dùng báo **3,3 V**. Hai bằng chứng khác cùng nói PA10 **thấp**:

| Bằng chứng | Kết luận |
|---|---|
| Analyzer **nhận frame** `0x400` / `0x402` có ACK; TEC = 0; mainboard ghi được `.blf` (bài 9) | TJA1044 **ở Normal**: ở standby bộ phát của nó tắt — không thể đặt một frame nào lên dây |
| `g_diag.can.stb_level` = **0** | thanh ghi IDR đọc **chính điện áp ở pad PA10**: dưới ngưỡng thấp |

Một đồng hồ 3,3 V ở PA10 mâu thuẫn với cả hai. **Giả thuyết mạnh nhất: que đo chạm chân bên cạnh.** LQFP32 bước chân 0,8 mm: **chân 20 = PA10**, **chân 19 = PA9 = USART1_TX**, mà TX **rảnh ở mức cao 3,3 V**. Hoặc số là của lần đo ở bài 1 (khi PA10 đúng là 3,3 V).

**Đo lại (1 phút), trong lúc frame đang đi ở analyzer:**
- **R2 chân 2** (phía MCU) so GND ⇒ mong ≈ 0 V;
- **R2 chân 1** = TJA1044 chân 8 (STB) ⇒ mong ≈ 0 V (có thể nhỉnh hơn vài chục mV nếu TJA1044 có pull-up nội trên STB — bài 0 của GĐ 1 gợi ý là có).

Nếu R2 chân 2 **vẫn 3,3 V trong lúc frame đi** ⇒ board khác netlist (EN_CAN1 không phải PA10, hoặc STB không nối như sơ đồ): **mở lại GĐ 4** để điều tra phần cứng. Giai đoạn đóng trên các tiêu chí chức năng, với dòng này để mở cho tới khi có số đo.

## 7. Vấn đề còn mở

1. **Đo lại PA10 và STB lúc phát** — mục 6.1.
2. **Bài 5 không có số** (`tec` cao nhất, `tx_aborted`, bước nhảy `Counter`): ghi PASS. Lần debug tới, đọc `g_diag.can.tx_aborted` sau một lần mất ACK để có mốc.
3. **Mất điện giữa lúc ghi flash**: chưa gây ra được có kiểm soát; phòng tuyến là thiết kế (page xoá xong thì trượt CRC) và 128 bit lật trên host.
4. **Bus-off thật** chưa từng xảy ra; `AutoBusOff` bật, `BUS_OFF_SEEN` chốt.
5. **`STM32F303K8TX_FLASH.ld` là file CubeMX sinh**: một lần Generate có thể trả `LENGTH` về 64K — `check_linker` bắt, nhưng IOC Guide nên có một mục "sau khi Generate, sửa linker" như module góc (B3 bên đó). Thêm khi làm GĐ 5 (cùng lúc dời `ORIGIN`).
6. Mở từ trước, vẫn mở: độ chính xác RH (không có ẩm kế); đo DE bằng oscilloscope; số `latency_ms`; chung âm 12 V / 5 V trong hệ data logger; proposal TH chưa có nhà ở `VF_Main`; hai lỗi chữ trong hợp đồng; watchdog.
7. *(Ngoài repo, `VF_Main`)* hai comment cũ ghi ngược TempHumi / TempRTD (`app_guidata.h:249`, `ConfigView.cpp:160`) — code đúng.
