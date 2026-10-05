# VF_TempHumi_Module — đề xuất hợp đồng CAN (loại `0x3`)

**Trạng thái: ĐÃ DUYỆT 2026-10-05, theo PHƯƠNG ÁN A** của mục 4 — loại `0x3` có chu kỳ phát cố định 1000 ms, `SET_TX_PERIOD` bị từ chối. Hợp đồng đã sinh ra: DBC v1.5, ID Map v1.3, CAN2 v1.5.

Đây là **giai đoạn 0** và nó chặn mọi thứ khác. ID và layout signal ràng buộc firmware của cả 10 module cùng loại; đổi về sau nghĩa là nạp lại tất cả.

Duyệt file này ⇒ sinh ra: `DBC v1.5`, `ID Map v1.3`, `CAN2 Protocol v1.5` ở repo `VF_Main`, rồi đồng bộ `contracts/` vào **cả ba** repo module.

---

## 1. Sự thật phần cứng phải nói trước mọi thứ khác

### 1.1 Cảm biến làm mới mỗi 2 giây

Datasheet SHT RS485, bảng "Thông số kỹ thuật":

| | |
|---|---|
| Thời gian làm mới nhiệt độ & độ ẩm | **2 s** |
| Thời gian đáp ứng | ≤ 25 s (nhiệt độ), ≤ 8 s (độ ẩm), ở gió 1 m/s |
| Độ phân giải | 0,1 °C · 0,1 %RH |
| Độ chính xác | ±0,5 °C (ở 25 °C) · ±4 %RH (ở 60 %RH, 25 °C) |
| Dải | −40…+60 °C · 0…100 %RH |
| Nguồn | 5…28 VDC, ≤ 0,05 W |

**Hỏi nhanh hơn 2 s không cho số mới.** Mọi quyết định dưới đây chảy ra từ dòng này.

### 1.2 Giao thức cảm biến — Modbus RTU

Mặc định xuất xưởng: **địa chỉ 1, baud 4800**, 8 data bits, 1 stop bit, **no parity**. Baud chọn được 2400 / 4800 / 9600.

| Thanh ghi | Nội dung | Kiểu |
|---|---|---|
| `0x0000` | **Độ ẩm** ×10 | read-only |
| `0x0001` | **Nhiệt độ** ×10, **bù 2 khi âm** | read-only |
| `0x07D0` | địa chỉ slave (1–254) | read/write |
| `0x07D1` | baud (0 = 2400, 1 = 4800, 2 = 9600) | read/write |

Đọc cả hai bằng **một** giao dịch: `addr 0x01`, `func 0x03`, start `0x0000`, count `0x0002` → trả `[humi_hi humi_lo temp_hi temp_lo]`. CRC là **CRC16-MODBUS, byte thấp trước**.

Ví dụ có số thật trong datasheet: `0x0102` = 258 → 25,8 %RH; `0xFF33` = −205 → −20,5 °C.

> **Giữ baud 4800 mặc định, đừng tối ưu.** Ở 2 s một lần đo, 4800 baud là thừa thãi — một giao dịch 8+9 byte hết khoảng 36 ms. Đổi cảm biến sang 9600 chỉ đổi lấy việc một cảm biến thay thế mới ra hộp sẽ **không nói chuyện được**.

### 1.3 DE/RE bị kéo lên +5 V trên board

Xem `CLAUDE.md` §4.3. Không ảnh hưởng hợp đồng CAN, nhưng ảnh hưởng mọi bài test — ghi lại ở đây để người duyệt thấy đủ bức tranh.

---

## 2. Phân bổ ID — suy ra máy móc từ quy tắc nibble

Không có gì để quyết ở mục này: ID Map §2 đã dành sẵn dải cho loại `0x3` từ v1.0.

```
node_id   = (module_type << 4) | (label - 1)      label 1..10
base      = 0x100 + node_id * 0x10
cmd_id    = 0x010 + node_id
```

| | |
|---|---|
| `module_type` | **`0x3`** |
| `node_id` | **`0x30` – `0x39`** |
| Khối dữ liệu | **`0x400` – `0x49F`** |
| Khối lệnh | **`0x040` – `0x049`** |

| Label | `node_id` | base | `TH_Data` | `TH_Status` | `TH_Cmd` |
|---|---|---|---|---|---|
| 1 | `0x30` | `0x400` | `0x400` | `0x402` | `0x040` |
| 2 | `0x31` | `0x410` | `0x410` | `0x412` | `0x041` |
| … | … | … | … | … | … |
| 10 | `0x39` | `0x490` | `0x490` | `0x492` | `0x049` |

**`base + 1` và `base + 3` để trống**, giữ đúng vị trí `Status` ở `base + 2` như hai loại trước. Một người đọc `.blf` quen với `0x302` là status của module góc sẽ tìm status của module này ở `0x402` mà không phải tra bảng.

---

## 3. Ngân sách bus — loại này gần như miễn phí

ID Map §2.2 cảnh báo: *"Hai loại module chưa đặc tả (`0x3`, `0x4`) phát chậm hơn nhiều… nhưng **phải tính**, đừng giả định nó giống module rung."*

Đã tính:

```
1 frame-slot  =  1 frame moi 10 ms  =  2,22% tai bus

TH_Data @ 1000 ms  =  1/100 frame-slot  =  0,01
TH_Status @ 1 Hz   =  nhu moi module khac, da nam trong ngan sach status
```

| Đội hình | Frame-slot |
|---|---|
| 1 module TempHumi | **0,01** |
| 10 module TempHumi | **0,1** |
| (so sánh) 1 module rung @ 10 ms | 1 |

**Mười module loại `0x3` tốn bằng 1/10 một module rung.** Trần vận hành 20 slot không bị loại này đụng tới.

---

## 4. ⚠️ Va chạm hợp đồng: `SET_TX_PERIOD` chỉ có **1 byte**

CAN2 Protocol §4.2:

```
| 0x12 | SET_TX_PERIOD | [1] ms: 5, 10, 20 hoac 50 - tuy loai |
```

Payload **một byte** ⇒ tối đa 255 ms. Module này muốn phát ở **1000 ms** — **không biểu diễn được**.

Đây là va chạm thật và phải quyết **trước khi** phiên code bắt đầu, không phải khi tới phase 5.

### Ba phương án

| | Phương án | Phạm vi ảnh hưởng |
|---|---|---|
| **A** ✅ | **Loại `0x3` có chu kỳ CỐ ĐỊNH 1000 ms.** `SET_TX_PERIOD` trả `BAD_PARAM` với mọi giá trị | Không đổi CAN2 protocol. `VF_Main`: màn hình CẤU HÌNH đã tự ẩn nút chu kỳ không hợp lệ (`period_ok[i]`), nhưng bước 3b sẽ không còn nút nào — cần một dòng "chu kỳ cố định 1 s" |
| B | Mở rộng `SET_TX_PERIOD` thành **1 hoặc 2 byte** (uint16 LE ms). 1 byte = như cũ | CAN2 v1.5 (cộng thêm, không phá vỡ). `VF_Main`: `app_modcfg_tx_period_ok()` nhận `uint16_t`, 4 nút chu kỳ theo loại, sổ kiểm kê ghi được > 255 |
| C | Đơn vị 100 ms **riêng cho loại `0x3`** | Rẻ nhất về code, **tệ nhất về sau**: sổ kiểm kê ghi `tx_period_ms` sẽ ghi sai số cho một loại module, và không gì trong frame nói đơn vị là gì |

### Đề xuất: **A**

Lý do không phải là tiết kiệm công:

> **Một cảm biến làm mới mỗi 2 giây không có chu kỳ phát đáng để cấu hình.** Mọi lựa chọn có nghĩa đều nằm trong khoảng 1–2 s, và sự khác nhau giữa chúng không thay đổi được gì trong phân tích. Cho người vận hành bốn nút để chọn giữa những thứ như nhau là mời họ nghĩ rằng lựa chọn đó quan trọng.

C bị loại thẳng: nó làm `tx_period_ms` trong sổ kiểm kê nói dối cho một loại module, và sổ kiểm kê là thứ được đọc khi module đã nằm trong hộp.

B là đường nâng cấp đúng **nếu** sau này có loại module cần chu kỳ chậm mà **có** lý do chọn. Giữ nó trong hồ sơ, đừng làm bây giờ.

> **Người duyệt cần trả lời đúng một câu: A hay B?** Chọn B thì `VF_Main` phải sửa thêm khoảng ba chỗ và CAN2 lên v1.5.

---

## 5. Message

Byte order **Intel (`@1+` / `@1-`)**, như toàn bộ DBC hiện có. DLC luôn **8**.

### 5.1 `TH_Data` — `base + 0`, chu kỳ TX

| Bit | Signal | Kiểu | Hệ số | Dải | Đơn vị |
|---|---|---|---|---|---|
| 0 \| 16 | `Temperature` | `@1-` | 0,1 | −40 … 60 | °C |
| 16 \| 16 | `Humidity` | `@1+` | 0,1 | 0 … 100 | %RH |
| 32 \| 16 | **`DataAge`** | `@1+` | 0,1 | 0 … 6553,5 | s |
| 48 \| 8 | `Counter` | `@1+` | 1 | 0 … 255 | — |
| 56 \| 8 | `Flags` | `@1+` | 1 | 0 … 255 | — |

`Temperature` và `Humidity` đi thẳng từ thanh ghi cảm biến — **cùng đơn vị ×10, không chuyển đổi**. Một phép nhân chia ít đi là một chỗ sai ít đi, và số trên bus khớp đúng số trong datasheet khi soi bằng tay.

#### `DataAge` là lý do tồn tại của cả message này

**Tuổi của số đo, tính từ lần Modbus trả lời thành công gần nhất.** `0xFFFF` = chưa đọc được lần nào kể từ khi khởi động.

Không có trường này thì một cảm biến chết — dây đứt, mất nguồn 12 V, cảm biến treo — vẫn cho ra `TH_Data` **đẹp đẽ, liên tục, với giá trị cuối cùng nó từng đọc được**. Trông y hệt một phòng ổn nhiệt.

Đó không phải lo xa. Đó là **lỗi đã xảy ra** trên module rung: `VIB_Status.Temperature` đóng băng giá trị lúc khởi động và phát lại mỗi giây mãi mãi; không ai phát hiện trong nhiều tuần, và mọi log trước 2026-09-26 không dùng được để xét trôi nhiệt (`VF_Main/docs/VF_Main_Remaining_Work.md` §4.3).

> Cách hỏng duy nhất mà loại module này thực sự có, là hỏng **im lặng**. `DataAge` là thứ làm nó hết im lặng — và nó phát hiện được **từ bên trong file `.blf`**, không cần ai nhớ ra để đi kiểm.

#### `Flags`

| Bit | Tên | Nghĩa |
|---|---|---|
| 0 | `NEVER_READ` | chưa có lần đọc thành công nào kể từ khi khởi động |
| 1 | `TIMEOUT` | giao dịch Modbus gần nhất không có trả lời |
| 2 | `CRC_ERR` | có trả lời nhưng CRC sai |
| 3 | `STALE` | `DataAge` > ngưỡng (đề xuất 6 s = 3 chu kỳ làm mới) |
| 4 | `RANGE` | giá trị ngoài dải ghi trong datasheet |
| 5 | `BUS_OFF_SEEN` | CAN đã vào bus-off ít nhất một lần |
| 6–7 | dự trữ | |

`NEVER_READ` tách riêng khỏi `STALE` có chủ ý: "chưa bao giờ nói chuyện được với cảm biến" và "đã nói chuyện được nhưng lâu rồi" là hai lỗi khác nhau, cần đi sửa hai chỗ khác nhau.

### 5.2 `TH_Status` — `base + 2`, 1 Hz **và ngay khi nhận lệnh**

| Bit | Signal | Kiểu | Nghĩa |
|---|---|---|---|
| 0 \| 8 | `FwVersion` | `@1+` | phiên bản firmware ứng dụng |
| 8 \| 8 | `Flags` | `@1+` | như trên |
| 16 \| 8 | `NodeId` | `@1+` | module tự khai, để bắt trùng ID |
| 24 \| 8 | `TxPeriod_x10ms` | `@1+` | 100 = 1000 ms |
| 32 \| 16 | `ModbusOk` | `@1+` | số giao dịch thành công, vòng lại |
| 48 \| 8 | `ModbusErr` | `@1+` | số lỗi, **bão hoà ở 255** |
| 56 \| 8 | `CmdSeq` | `@1+` | echo `Seq` của lệnh vừa nhận |

`ModbusErr` bão hoà chứ không vòng lại: "255 lỗi" đọc là *hỏng nặng*, còn một bộ đếm vòng về 3 đọc là *gần như ổn*.

### 5.3 `TH_Cmd` — `0x040 + node_id`

**Kế thừa nguyên vẹn** định dạng chung ở ID Map §2.3: `Cmd`, `Seq`, `Arg0..3`, magic `0x5A` `0xA5`, DLC 8. Ba ràng buộc ở §2.3 (chống thực thi hai lần, magic, echo `Seq`) áp dụng đầy đủ.

| `Cmd` | Tên | Tác dụng trên loại `0x3` |
|---|---|---|
| `0x00` | `PING` | trả `TH_Status` ngay |
| `0x01`–`0x03` | `ZERO`, `CLEAR_ZERO`, `GYRO_BIAS_CAL` | **`BAD_CMD`** — không có nghĩa với loại này |
| `0x10` | `FORCE_READ` | hỏi cảm biến ngay, không chờ hết chu kỳ |

`FORCE_READ` có ích đúng một chỗ: lúc lắp đặt, để biết ngay dây RS485 đã thông chưa mà không phải đợi tới một giây.

---

## 6. Thay đổi cụ thể trên từng file hợp đồng

### 6.1 `dbc/VF_E-Drive_Data_Logger_v1.5.dbc` — **file mới**

Theo `dbc/README.md`: **không sửa tại chỗ**, mỗi lần đổi là một file mới với `x+1`.

Cộng thêm, không đụng một ID nào đang có:

```
BU_: ... TH_01 TH_02 ... TH_10

BO_ 1024 TH_Data_01: 8 TH_01
 SG_ Temperature : 0|16@1- (0.1,0) [-40|60] "degC" Vector__XXX
 SG_ Humidity : 16|16@1+ (0.1,0) [0|100] "%RH" Vector__XXX
 SG_ DataAge : 32|16@1+ (0.1,0) [0|6553.5] "s" Vector__XXX
 SG_ Counter : 48|8@1+ (1,0) [0|255] "" Vector__XXX
 SG_ Flags : 56|8@1+ (1,0) [0|255] "" Vector__XXX

BO_ 1026 TH_Status_01: 8 TH_01
 ... (7 signal như §5.2)

BO_ 64 TH_Cmd_01: 8 Vector__XXX
 ... (định dạng chung §2.3)
```

`0x400` = 1024, `0x402` = 1026, `0x040` = 64. Nhân lên cho 10 module: `+16` mỗi bước ở khối dữ liệu, `+1` ở khối lệnh.

Kèm `CM_` cho `DataAge` và từng bit của `Flags` — đó là chỗ người phân tích sẽ đọc.

### 6.2 `docs/VF_Main_CAN_ID_Map.md` → **v1.3**

| Mục | Đổi gì |
|---|---|
| §2.1 bảng dải | `0x3` **chưa đặt tên** → **Nhiệt–ẩm (`VF_TempHumi_Module`)** |
| §2.1 | thêm bảng chi tiết 10 module, như bảng module rung và module góc |
| §2.2 | thêm dòng frame-slot: loại nhiệt–ẩm @ 1000 ms = **0,01** |
| §2.2 | sửa câu "hai loại chưa đặc tả" → còn **một** loại (`0x4`) |

### 6.3 `docs/VF_Main_CAN2_Service_Protocol.md`

**Phương án A: không đổi version**, chỉ thêm một dòng vào bảng §4.2 — loại `0x3` không hỗ trợ giá trị nào của `SET_TX_PERIOD`, chu kỳ cố định 1000 ms.

*(Phương án B thì lên **v1.5** và mở rộng payload thành 1–2 byte.)*

### 6.4 Firmware `VF_Main` — bắt buộc, cùng lúc

| File | Đổi gì |
|---|---|
| `Core/Inc/app/app_config.h` | `APP_MODCFG_TYPES_IMPLEMENTED` thêm bit 3 |
| `Core/Src/app/app_modreg.c` | `app_modreg_type_str()`: `"TEMPHUMI"`; `app_modreg_state_str()` giải mã `Flags` cho loại 3 |
| `Core/Src/app/app_modcfg.c` | tập chu kỳ hợp lệ cho loại 3 (phương án A: rỗng) |
| `TouchGFX/gui/src/config_screen/ConfigView.cpp` | bước 3b khi không có nút chu kỳ nào hợp lệ |

**Bỏ bước này là hỏng dữ liệu im lặng**: module vẫn phát, `.blf` vẫn mở được, chỉ là màn hình gọi nó là `TYPE 3` và DBC không giải mã được gì.

### 6.5 Đồng bộ `contracts/` — **cả ba repo**

```
VF_Vibration_Module/contracts/
VF_Angle_Module/contracts/
VF_TempHumi Module/contracts/     <- tạo mới
```

Bỏ `VF_E-Drive_Data_Logger_v1.4.dbc`, thêm `v1.5`, cập nhật `SHA256SUMS`.

> ⚠️ **Giữa lúc bump hợp đồng và lúc đồng bộ xong, build của hai repo module kia sẽ FAIL.** Đó là hành vi đúng của `check_contracts.sh`, nhưng nó có nghĩa là bước này phải làm **liền một mạch**, không bỏ dở giữa chừng.

---

## 7. Những gì đề xuất này CỐ TÌNH không làm

| | Vì sao |
|---|---|
| Không cấu hình baud/địa chỉ cảm biến qua CAN2 | Mặc định xuất xưởng chạy được. Thêm lệnh để đổi thứ không cần đổi là thêm một cách làm hỏng một cảm biến đang tốt từ xa |
| Không có `TH_Rate` hay message chẩn đoán thứ hai | `ModbusOk`/`ModbusErr` trong `TH_Status` đã đủ. Module góc có `ANG_Rate` **mặc định tắt** và nó chưa bao giờ được bật |
| Không bù nhiệt, không lọc, không tính điểm sương | Cảm biến đã cho số đã hiệu chuẩn. Mọi phép tính thêm làm trên PC, nơi sửa được mà không phải nạp lại 10 module |
| Không hỗ trợ nhiều cảm biến trên một module | Một module, một cảm biến. Nhiều cảm biến thì cắm nhiều module — rẻ hơn và dải ID đã có sẵn 10 chỗ |
