# VF_TempHumi_Module — Giai đoạn 5: quy trình test trên board

**Image:** hai image từ cùng một commit.

| | `.elf` | Định danh |
|---|---|---|
| Bootloader | `VF_TempHumi_Bootloader/Debug/VF_TempHumi_Bootloader.elf` | `s_boot_id` = `"VF_TempHumi_Bootloader 0.1.0"`; `HELLO` byte 5..7 = `00 01 00` |
| Ứng dụng | `Debug/VF_TempHumi_Module.elf` | `s_fw_id` = `"VF_TempHumi_Module phase 5"`, `g_diag.fw_phase` = 5 |

Giai đoạn 5 chứng minh:

1. **Bootloader nhả RS485 từ đầu và giữ nó thấp qua cú nhảy** — PB6 ≈ 0 V cả lúc ở bootloader.
2. **Chính sách lúc cấp nguồn** (CAN2 §5 bất biến 5): không ứng dụng ⇒ ở lại mãi; config không hợp lệ ⇒ ở lại mãi; còn lại ⇒ nghe ≥ 1000 ms rồi nhảy.
3. **Giao thức `0x01`–`0x30`** cho loại `0x03`, đúng mã trả về, `RESET` trả lời trước.
4. **Cấu hình một module trắng từ đầu tới lúc phát** trên CAN1, không nạp lại gì.
5. SWD sống sót với bootloader chạy trước.

---

## 0. Trước khi test: build — người dùng làm một lần

### 0.1 Dời ứng dụng ra sau bootloader — **bạn sửa tay** (kế hoạch GĐ 5, câu hỏi 3)

**Đây là lúc "khi tôi báo" của câu hỏi 3.** Trong `STM32F303K8TX_FLASH.ld` **ở gốc repo** (của ứng dụng, không phải của bootloader), đổi đúng một dòng:

```
  FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 50K      <- truoc
  FLASH    (rx)    : ORIGIN = 0x8003000,   LENGTH = 50K      <- sau
```

Giữ nguyên `LENGTH = 50K`. Từ lúc này **ứng dụng không còn chạy được một mình** — luôn nạp kèm bootloader.

### 0.2 Build cả hai trong IDE

1. Chọn project `VF_TempHumi_Bootloader` → **Refresh (F5)** → **Project → Build Project**. Lần đầu bắt buộc qua IDE (CLAUDE.md §3: `Debug/makefile` chưa tồn tại).
2. Chọn `VF_TempHumi_Module` → **Clean** → **Build** (linker script đổi ⇒ phải link lại).
3. Dán **cả hai** log build. Mong đợi:

| | Mong đợi |
|---|---|
| Đầu log ứng dụng | `check_contracts: ok, 4 files match VF_Main.` và **`check_linker: ok (application, behind the bootloader), FLASH 0x08003000 .. 0x0800F800`** — nếu vẫn ghi `ONE IMAGE` ⇒ bước 0.1 chưa làm |
| Đầu log bootloader | `check_contracts: ok …` và `check_linker: ok (bootloader), FLASH 0x08000000 .. 0x08003000` |
| Lỗi / cảnh báo | **0 / 0** cả hai |
| Kích thước bootloader | `text + data` **≤ 12 288**. Build thử trên Linux (`arm-none-eabi-gcc` 13.2, cùng cờ `-Og`) cho **10 112 B (82 %)** — toolchain ST 14.3 có thể lệch vài trăm byte. Tràn ⇒ linker báo `region FLASH overflowed`: báo lại, đừng tự đổi sang `-Os` |
| Kích thước ứng dụng | ~28–29 KB / 50 KB |

> **Bootloader include `${workspace_loc:/VF_TempHumi_Module/Core/Inc}`** — project ứng dụng phải có tên đúng `VF_TempHumi_Module` trong workspace. Lỗi `app/app_config.h: No such file` ⇒ kiểm tên project.

---

## 1. Chuẩn bị

- **Nguồn** như GĐ 2–4: +5 V, +12 V, **chung âm**, cảm biến ở J1. Nạp khi chưa cấp 12 V (JP2 chân 5/6 cạnh SWD).
- **STM32CubeProgrammer** để nạp và xoá. ⛔ **KHÔNG BAO GIỜ "Full chip erase"** — nó xoá luôn page config (`0x0800F800`). Chỉ "Erase selected sectors".
- Bản đồ page (2 KB mỗi page) trong CubeProgrammer:

| Page | Địa chỉ | Vùng |
|---|---|---|
| 0 – 5 | `0x08000000` – `0x08002FFF` | bootloader |
| 6 – 30 | `0x08003000` – `0x0800F7FF` | ứng dụng |
| **31** | `0x0800F800` – `0x0800FFFF` | **config** |

- **Analyzer** (CANoe / TSMaster…) ở chế độ **thường — có ACK**, 500 kbit/s, DBC v1.5. Chuẩn bị sẵn các frame gửi tay trên **`0x7E0`, DLC 8** (bảng ở bài 4), và một frame `HELLO` **gửi tuần hoàn 100 ms** để bật/tắt.
- **Bus cô lập cho bài 1–5, 7–8**: analyzer + module + 120 Ω. Không mainboard, không module khác.
- Start measurement ở analyzer **trước** khi cấp nguồn module.
- Live Expressions: ứng dụng — `g_diag.app_base`, `g_diag.cfg`, `g_diag.can`; bootloader — **`g_boot`** (chỉ xem được khi debug project bootloader).

| `g_boot` | Nghĩa |
|---|---|
| `magic` | `826427476` = `0x31424854` "THB1" |
| `app_present` | 1 = có bảng vector hợp lý ở `0x08003000` |
| `cfg_state` | như `g_diag.cfg.state`: 0 OK · 1 trắng · 2 magic · 3 version · 4 CRC · 5 loại · 6 node · 7 reserved |
| `can_start` | 0 OK · 2 filter · 3 `HAL_CAN_Start` |
| `policy` | **0 nghe rồi nhảy · 1 đường cứu (không ứng dụng) · 2 chưa cấu hình** |
| `heard` | 1 = đã nhận ít nhất một `0x7E0` |
| `requests`, `responses_ok`, `responses_lost`, `rx_errors` | bộ đếm; `last_cmd` / `last_status` = request cuối |

### Hai điều khác GĐ 4 — đừng báo là lỗi

1. **Module chưa cấu hình giờ ở trong bootloader, PA10 ≈ 0 V (transceiver Normal)** và ACK frame trên bus. GĐ 4 bài 1 thấy PA10 ≈ 3,3 V vì lúc đó ứng dụng chạy và đứng standby. Đây là quyết định đã duyệt (câu hỏi 1; CLAUDE.md §6.5): CAN2 §5.1/§5.3 buộc module config trắng phải trả lời. **Cái không được đổi:** module chưa cấu hình **không phát gì** nếu không ai hỏi — không `0x7E8`, không `TH_*`.
2. **Công tắc `APP_CFG_BENCH_WRITE` không còn cứu được một page trắng** khi khởi động bình thường: bootloader ở lại mãi, ứng dụng không bao giờ chạy. Cấu hình module trắng bây giờ là **qua CAN2** (bài 5). (Debug launch từ IDE chạy thẳng ứng dụng, bỏ qua bootloader — nên công tắc vẫn dùng được kiểu đó.)

---

## 2. Các bài

### Nạp lần đầu — thứ tự bắt buộc

CubeProgrammer, Connect, **không** Full chip erase:

1. **Erase selected sectors 0–30** (bootloader + ứng dụng cũ; **chừa page 31**). Lý do: ứng dụng GĐ 4 nằm ở `0x08000000`; nạp đè chỉ xoá những page image mới chạm tới, và phần đuôi image cũ ở page 6+ có thể trông giống một bảng vector.
2. **Open file** `VF_TempHumi_Bootloader.elf` → Download. (`.elf` mang sẵn địa chỉ. Dùng `.bin` thì phải gõ địa chỉ `0x08000000`.)
3. **Open file** `VF_TempHumi_Module.elf` → Download (vào `0x08003000`).
4. Disconnect, tắt/bật nguồn.

Page 31 vẫn giữ config bench `0x30` từ GĐ 4 bài 2/6.

### Bảng bài

| # | Làm gì | Mong đợi | Nếu khác |
|---|---|---|---|
| 0 | 12 V ở J1, chung âm | như GĐ 2 bài 0 | |
| **1** | **Khởi động bình thường.** Analyzer chạy trước, cấp nguồn module | LED: **nháy đôi** (50 sáng / 150 tắt / 50 sáng) **đúng một lần ~1 s**, rồi 2 Hz của ứng dụng, rồi 1 nháy / 5 s. Analyzer: **không frame nào từ module trong ~1 s đầu**, rồi `TH_Data_01` (`0x400`) / `TH_Status_01` (`0x402`) mỗi 1 s như GĐ 4. Debug ứng dụng (attach): **`g_diag.app_base` = `134230016`** (`0x08003000`), `fw_phase` 5, `cfg.state` 0 | LED nháy đôi **mãi** ⇒ bootloader không nhảy: debug project bootloader, đọc `g_boot.app_present` / `cfg_state` / `policy`. Board tối hẳn ⇒ nhảy rồi chết: gửi `g_boot` + ảnh `SCB->VTOR` |
| **2** | **PB6 trong bootloader.** Bật `HELLO` tuần hoàn 100 ms ở analyzer, **rồi** bấm SW1. Thấy `0x7E8` thì tắt `HELLO` tuần hoàn. Đo PB6 so GND, rồi PA10 | Module **ở lại** (phiên): LED nháy đôi **mãi**, không `0x400`. **PB6 ≈ 0 V** (R8/R9 kéo 5 V mà vẫn thấp — như GĐ 1). PA10 ≈ 0 V (Normal). *(Oscilloscope, tuỳ chọn: PB6 + 3V3, cấp nguồn **không** có `HELLO` ⇒ PB6 xuống trong vài ms đầu và **không nhảy lên lúc chuyển sang ứng dụng** ~1 s sau)* | **PB6 ≈ 5 V ⇒ DỪNG**, gửi ngay — đó là MAX485 giữ bus RS485 |
| **3** | **Đường cứu — bài quan trọng nhất.** CubeProgrammer: **Erase selected sectors 6–30** (chỉ ứng dụng). Tắt/bật. Đợi **10 s**, rồi gửi `HELLO` một lần | LED **nháy đôi mãi** (không mã 3: config vẫn hợp lệ — đây là trường hợp "không ứng dụng"). Analyzer: **không frame nào** trong 10 s chờ. `HELLO` ⇒ `0x7E8` = **`01 00 01 03 30 00 01 00`**. Xong: nạp lại **chỉ** `VF_TempHumi_Module.elf`, tắt/bật ⇒ như bài 1 | Không trả lời ⇒ đường cứu hỏng: debug bootloader, gửi `g_boot` |
| **4** | **Toàn bộ giao thức** (module đã cấu hình — vào phiên như bài 2). Gửi lần lượt bảng 4a, ghi `0x7E8` và **khoảng thời gian `0x7E0` → `0x7E8`** (timestamp analyzer) cho từng dòng | Đúng bảng 4a. Mọi trả lời **< 50 ms**; **`COMMIT` < 50 ms** (xoá page — lần đầu đo trên chip này). Sau `RESET`: thấy `30 00 …` **trước**, ~1 s sau module phát **`0x430` / `0x432`** (`NodeId` 51 trong `TH_Status_03`) | `COMMIT` > 50 ms ⇒ ghi lại số đo, vẫn đi tiếp (mainboard thử lại 3 lần) |
| 4b | Vào phiên lại, `SET_NODE_ID 0x30`, `COMMIT`, `RESET` | về `0x400` / `0x402` | |
| **5** | **Module trắng, cấu hình từ đầu.** CubeProgrammer: **Erase selected sectors 31** (chỉ config). Tắt/bật trên bus có analyzer, **không gửi gì 30 s** | LED **mã 3** (3 nháy, nghỉ 2 s), **mãi**. Analyzer: **0 frame từ module** trong 30 s — không `0x7E8`, không `TH_*` — kể cả lúc cấp nguồn. PA10 ≈ 0 V (xem mục 1, điều khác 1) | **Có frame không ai hỏi ⇒ DỪNG**, gửi ảnh trace |
| 5b | Gửi `HELLO` | `01 00 01 03 **FF** 00 01 00`; **LED chuyển sang nháy đôi** ngay | |
| 5c | `SET_TX_PERIOD 10`, rồi `COMMIT` (chưa có node) | `12 02 …`; `COMMIT` ⇒ **`20 02 …`** (không ghi gì). `HELLO` vẫn `FF` | `20 00` ⇒ đã ghi một config không có node: dừng |
| 5d | `SET_NODE_ID 0x30`, `COMMIT`, `HELLO`, `RESET` | `10 00`, `20 00`, `HELLO` ⇒ `… 30 …`, `30 00`. ~1 s sau: **`0x400` / `0x402` trên dây**, `g_diag.cfg.node_id_source` = **2** (CONFIG). **Module trắng → đo được, không nạp lại gì** | |
| **6** | **Mainboard thật qua J2** — chỉ khi có dây chuyển (câu hỏi 5). Rút module khỏi CAN1 **trước** (CAN2 §1: không bao giờ cắm cả hai). `mod open` / màn hình CẤU HÌNH → chọn TempHumi → **bấm SW1 trên module trong 10 s** (module đã cấu hình) → nhãn 1 → `COMMIT`. Rút, cắm lại CAN1 | Mainboard: `HELLO` thấy loại 3, UID có; `COMMIT` OK; sổ kiểm kê có dòng mới. Trên CAN1: `0x400` / `0x402`. *(Thử thêm: nhãn 4 ⇒ `0x430`, rồi trả về nhãn 1)* | `no answer from the module` ⇒ module đã ở ứng dụng: bấm SW1 **sau** `mod open`. `nothing acknowledged` ⇒ dây / nguồn |
| 7 | Tắt/bật 3 lần; mỗi lần CubeProgrammer **Connect** (ST-Link, mode Normal) | vào được mỗi lần | Không vào ⇒ thử "Under reset", ghi lại |
| 8 | **Debug ứng dụng từ CubeIDE** (Run → Debug), dừng, rồi tắt/bật **không** debugger | lúc debug chạy bình thường (`app_base` vẫn `0x08003000`); tắt/bật ⇒ **vẫn thấy nháy đôi ~1 s** (debug launch không xoá bootloader) | Không còn nháy đôi ⇒ launch config đang xoá cả chip: báo lại |
| 9 | **Hồi quy GĐ 2–4** + **việc mở của GĐ 4**: đo PA10 (R2 chân 2) **lúc đang phát** | đọc cảm biến, `DataAge`, mất ACK (GĐ 4 bài 4–5), filter. **PA10 ≈ 0 V** khi `0x400` đang lên dây | 3,3 V mà frame vẫn lên dây ⇒ đo nhầm điểm: đo ngay chân 8 TJA1044 |
| 10 | *(Tuỳ chọn, oscilloscope)* **Cửa sổ nghe**: kênh 1 = 3V3, kênh 2 = PA10. Cấp nguồn, không `HELLO` | PA10 xuống thấp (bootloader, Normal) → **lên** lúc nhảy (standby) → xuống lại (ứng dụng). **Độ rộng xung thấp đầu ≥ 1000 ms** (CAN2 §5 bất biến 5) | |

### 4a. Request (`0x7E0`, DLC 8, phần thừa `00`) → response (`0x7E8`)

| # | Request | Response mong đợi | Kiểm gì |
|---|---|---|---|
| 1 | `01 00 00 00 00 00 00 00` HELLO | `01 00 01 03 30 00 01 00` | proto 1, loại 3, node `0x30`, bootloader 0.1.0 |
| 2 | `02 …` GET_UID_LO | `02 00 u0 u1 u2 u3 u4 u5` | so với CubeProgrammer, **Memory** `0x1FFFF7AC`, độ rộng **8-bit**: byte 0–5 |
| 3 | `03 …` GET_UID_HI | `03 00 u6 … u11` | byte 6–11 |
| 4 | `10 25 …` SET_NODE_ID `0x25` | `10 02 …` | nibble cao 2 ≠ loại 3 |
| 5 | `10 3A …` | `10 02 …` | nibble thấp > 9 |
| 6 | `10 33 …` | `10 00 …` | |
| 7 | `11 02 …` SET_MODULE_TYPE 2 | `11 02 …` | firmware chỉ làm loại 3 |
| 8 | `11 03 …` | `11 00 …` | |
| 9 | `12 0A …` SET_TX_PERIOD 10 | `12 02 …` | tập rỗng (CAN2 §4.2) |
| 10 | `13 04 …` SET_FULL_SCALE | `13 01 …` | không hỗ trợ (câu hỏi 4) |
| 11 | `14 02 …` SET_GYRO_FS | `14 01 …` | |
| 12 | `40 …` | `40 01 …` | nạp firmware chưa đặc tả |
| 13 | `01 …` HELLO | `01 00 01 03 **30** …` | `SET` chưa `COMMIT` ⇒ HELLO vẫn báo node đang lưu |
| 14 | `20 …` COMMIT | `20 00 …` — **ghi thời gian** | |
| 15 | `01 …` HELLO | `01 00 01 03 **33** …` | |
| 16 | `30 …` RESET | `30 00 …` **rồi** module khởi động lại | trả lời trước (bất biến 4) |

Đừng gửi `0x7E0` trong ~1 s ngay sau `RESET` nếu muốn module nhảy sang ứng dụng — nó sẽ ở lại phiên (đúng hợp đồng).

---

## 3. Bảng ghi kết quả

| # | Kết quả (PASS / FAIL + ghi chú) |
|---|---|
| 0.2 | bootloader `text+data` ___ B, ứng dụng ___ B, cảnh báo ___ |
| 1 | nháy đôi ~1 s: có / không, `app_base` ___, `0x400` ___ Hz |
| 2 | PB6 ___ V, PA10 ___ V |
| 3 | `0x7E8` = ___ , frame trong 10 s chờ: ___ |
| 4 | khớp bảng 4a: ___ / 16, `COMMIT` ___ ms, trả lời chậm nhất ___ ms, sau RESET: `0x430` có / không |
| 5 | LED mã 3: có / không, frame trong 30 s: ___ ; 5b `HELLO` = ___ ; 5c `COMMIT` = ___ ; 5d `0x400` có / không, `source` ___ |
| 6 | màn hình: ___ |
| 7 | SWD 3/3: ___ |
| 8 | |
| 9 | PA10 lúc phát ___ V |
| 10 | cửa sổ nghe ___ ms |

## 4. Những gì quy trình này CHƯA kiểm được

- **Mất điện giữa `COMMIT`** — không gây ra được có kiểm soát. Phòng tuyến: page xoá xong thì trượt CRC ⇒ lần cấp nguồn sau bootloader ở lại (chưa cấu hình), cấu hình lại được qua CAN2; test host `test_app_cfg` (bit lật) và `test_boot_svc` (flash hỏng theo lệnh ⇒ `0x03`, `HELLO` báo trang thật).
- **`COMMIT` báo `0x03` trên chip** — không làm hỏng flash có chủ đích được; chỉ test host.
- **Ứng dụng cụt** (image bị cắt ngắn): `boot_policy_app_present` chỉ kiểm hợp lý bảng vector, không có CRC — đóng cùng lúc với nạp firmware qua CAN2 (CAN2 §7).
- **Nạp firmware qua CAN2** (`0x40`+) — chưa đặc tả.
