# Phân bổ Message ID trên CAN1

**Trạng thái: v1.3 — ĐÃ DUYỆT 2026-10-05.** Bảng này ràng buộc firmware của mọi module. Đổi về sau nghĩa là nạp lại toàn bộ module đã xuất xưởng.

> **v1.3 so với v1.2 — cộng thêm, không phá vỡ.** Đặt tên loại `0x3` là **module nhiệt–ẩm** (`VF_TempHumi_Module`, `node_id` `0x30`–`0x39`, khối `0x400`–`0x49F`), thêm bảng chi tiết 10 module (mục 2.1) và dòng frame-slot của nó (mục 2.2). Không một ID nào của v1.2 đổi chỗ. Đi kèm `dbc/VF_E-Drive_Data_Logger_v1.5.dbc`.
>
> Loại này **không có chu kỳ phát cấu hình được**: cảm biến SHT RS485 chỉ làm mới mỗi 2 giây nên mọi lựa chọn có nghĩa đều như nhau. `SET_TX_PERIOD` bị từ chối — xem `VF_Main_CAN2_Service_Protocol.md` §4.2.
>
> **v1.2 so với v1.1 — cộng thêm, không phá vỡ.** Đặt tên loại `0x2` là **module đo góc** (`VF_Angle_Module`, `node_id` `0x20`–`0x29`, khối `0x300`–`0x39F`), **định nghĩa lần đầu định dạng frame lệnh** trên dải `0x010 + node_id` (mục 2.3), và **viết lại ngân sách bus theo frame-slot** vì module góc phát 2 frame mỗi chu kỳ chứ không phải 1 (mục 2.2). Không một ID nào của v1.1 đổi chỗ. Đi kèm `dbc/VF_E-Drive_Data_Logger_v1.2.dbc` và `VF_Main_CAN2_Service_Protocol.md` v1.3.
>
> **v1.1 thay bảng `node_id` của v1.0.** Dự án chốt **40 sensor = 4 loại × 10 module**, mỗi loại một dải riêng (mục 2.1). Module rung chuyển từ `0x00`–`0x09` sang **`0x10`–`0x19`**, nên khối dữ liệu của nó dời từ `0x100` lên `0x200`.

CAN1 chạy 500 kbps, ID chuẩn 11 bit (`0x000`–`0x7FF`). ID càng nhỏ ưu tiên càng cao khi hai node phát cùng lúc.

## 1. Bảng dải

| Dải | Số ID | Dùng cho | Ai phát |
|---|---|---|---|
| `0x000`–`0x00F` | 16 | Dự trữ, ưu tiên cao nhất. Chưa dùng | — |
| `0x010`–`0x05F` | 80 | Lệnh điều khiển tới từng node: `0x010 + node_id` | mainboard |
| `0x060`–`0x0FF` | 160 | Dự trữ cho điều khiển | — |
| `0x100`–`0x5FF` | 1280 | **Dữ liệu module**: `0x100 + node_id × 0x10` | module |
| `0x600`–`0x60F` | 16 | **GPS** (mainboard sinh, phát lên bus, ghi log cờ TX) | mainboard |
| `0x610`–`0x61F` | 16 | Trạng thái của chính mainboard. Chưa dùng | mainboard |
| `0x620`–`0x7FF` | 480 | Dự trữ | — |

**`node_id` tối đa là `0x4F`, và đây là giới hạn cứng.** `node_id = 0x50` cho ra base `0x100 + 0x500 = 0x600`, tức đúng `GPS_Position`. Không có dải đệm nào giữa dữ liệu module và GPS nữa: dải dự trữ `0x500`–`0x5FF` của v1.0 đã bị loại module thứ tư dùng hết. Muốn nhiều hơn 80 node thì phải dời GPS hoặc đổi sang ID 29 bit — cả hai đều là thiết kế lại, không phải chỉnh số.

## 2. Cách tính ID của một module

Mỗi module mang một `node_id` từ `0x00` đến `0x4F` (80 node), gán qua CAN2 — xem `VF_Main_CAN2_Service_Protocol.md`.

```
base   = 0x100 + node_id * 0x10
message N của module  =  base + N        (N = 0x0 .. 0xF)
lệnh gửi tới module   =  0x010 + node_id
```

16 message cho mỗi module, 80 node. Đọc bằng hex là ra ngay: node `0x15` có base `0x250`, message thứ ba là `0x252`, lệnh tới nó là `0x025`.

Dải dữ liệu dừng đúng ở `0x5FF`, sát ngay dưới dải GPS `0x600` — xem cảnh báo giới hạn cứng ở mục 1.

**Node chưa cấu hình không có ID và phải im lặng trên CAN1.** `node_id = 0xFF` hoặc config sai CRC đều là chưa cấu hình. Lý do ở CLAUDE.md mục 1.2: hai node cùng ID phát cùng lúc gây bit error và có thể đẩy cả hai vào bus-off.

## 2.1 `node_id` đã phân bổ — 4 loại module, mỗi loại 10

Dự án có **40 sensor**, chia thành **4 loại module, mỗi loại 10 module**. Mỗi loại chiếm một nibble cao riêng:

```
node_id = (module_type << 4) | (số trên nhãn − 1)          nhãn = 1..10  →  nibble thấp 0x0..0x9
```

| `module_type` | Loại module | `node_id` | Khối dữ liệu | Lệnh tới module |
|---|---|---|---|---|
| `0x1` | **Rung** (`VF_Vibration_Module`) | `0x10`–`0x19` | `0x200`–`0x29F` | `0x020`–`0x029` |
| `0x2` | **Góc** (`VF_Angle_Module`) | `0x20`–`0x29` | `0x300`–`0x39F` | `0x030`–`0x039` |
| `0x3` | **Nhiệt–ẩm** (`VF_TempHumi_Module`) | `0x30`–`0x39` | `0x400`–`0x49F` | `0x040`–`0x049` |
| `0x4` | *chưa đặt tên* | `0x40`–`0x49` | `0x500`–`0x59F` | `0x050`–`0x059` |

Mỗi loại được **trọn một khối `0x100`** trong dải dữ liệu. Nhìn ID là biết ngay loại: `0x2xx` là **rung**, `0x3xx` là **góc**, `0x4xx` là **nhiệt–ẩm**, `0x5xx` để dành. Đây là lý do chọn cách chia theo nibble chứ không đánh số liên tiếp.

Chia theo nibble cũng là thứ làm cho việc thêm một loại module mới trở thành **thay đổi cộng thêm**: DBC v1.2 thêm trọn khối `0x300`–`0x39F` mà không một ID nào của module rung phải nhúc nhích, nên firmware rung không phải sửa dòng nào.

### Bất biến: nibble cao **là** `module_type`

Module phải từ chối (`0x02`) một `SET_NODE_ID` mà `node_id >> 4` khác `module_type` của chính nó, và từ chối cả nibble thấp lớn hơn `0x9`. Một phép kiểm duy nhất:

```c
ok = ((node_id >> 4) == module_type) && ((node_id & 0x0Fu) <= 0x09u);
```

Từ chối **ồn ào** ngay lúc gán, chứ không nhận bừa. Một module rung nhận nhầm `node_id = 0x25` sẽ phát ở `0x350`/`0x351` — đó là chỗ của **module loại 2**, nên nó vừa phát ra thứ DBC mô tả sai, vừa có thể đụng độ với một module loại 2 thật. Trùng ID làm sập bus, không chỉ làm rối log (mục 2).

Vì `VIB_Status` lặp lại `node_id` trong payload, sai lệch loại còn phát hiện được **từ chính file `.blf`**, không cần cắm lại module.

### Bảng chi tiết — module rung

| Nhãn trên vỏ | `node_id` | base | `VIB_Data` | `VIB_Status` | Lệnh tới module |
|---|---|---|---|---|---|
| Module 1 | `0x10` | `0x200` | `0x200` | `0x201` | `0x020` |
| Module 2 | `0x11` | `0x210` | `0x210` | `0x211` | `0x021` |
| Module 3 | `0x12` | `0x220` | `0x220` | `0x221` | `0x022` |
| Module 4 | `0x13` | `0x230` | `0x230` | `0x231` | `0x023` |
| Module 5 | `0x14` | `0x240` | `0x240` | `0x241` | `0x024` |
| Module 6 | `0x15` | `0x250` | `0x250` | `0x251` | `0x025` |
| Module 7 | `0x16` | `0x260` | `0x260` | `0x261` | `0x026` |
| Module 8 | `0x17` | `0x270` | `0x270` | `0x271` | `0x027` |
| Module 9 | `0x18` | `0x280` | `0x280` | `0x281` | `0x028` |
| Module 10 | `0x19` | `0x290` | `0x290` | `0x291` | `0x029` |

**Nhãn trên vỏ phải in cả hai số**, ví dụ `Module 3 (node 0x12)`. Quy đổi không còn là "trừ 1" như v1.0 mà là "trừ 1 rồi cộng nibble loại" — không ai nên phải tính nhẩm việc đó khi đang đứng cạnh xe.

### Bảng chi tiết — module góc

Loại `0x2`. Phát **hai** frame dữ liệu mỗi chu kỳ, khác module rung chỉ phát một — xem ngân sách bus ở mục 2.2. Thiết kế đầy đủ ở `docs/VF_Angle/VF_Angle_CAN_Proposal.md`.

| Nhãn trên vỏ | `node_id` | base | `ANG_Data` | `ANG_Accel` | `ANG_Status` | `ANG_Rate` | Lệnh |
|---|---|---|---|---|---|---|---|
| Angle 1 | `0x20` | `0x300` | `0x300` | `0x301` | `0x302` | `0x303` | `0x030` |
| Angle 2 | `0x21` | `0x310` | `0x310` | `0x311` | `0x312` | `0x313` | `0x031` |
| Angle 3 | `0x22` | `0x320` | `0x320` | `0x321` | `0x322` | `0x323` | `0x032` |
| Angle 4 | `0x23` | `0x330` | `0x330` | `0x331` | `0x332` | `0x333` | `0x033` |
| Angle 5 | `0x24` | `0x340` | `0x340` | `0x341` | `0x342` | `0x343` | `0x034` |
| Angle 6 | `0x25` | `0x350` | `0x350` | `0x351` | `0x352` | `0x353` | `0x035` |
| Angle 7 | `0x26` | `0x360` | `0x360` | `0x361` | `0x362` | `0x363` | `0x036` |
| Angle 8 | `0x27` | `0x370` | `0x370` | `0x371` | `0x372` | `0x373` | `0x037` |
| Angle 9 | `0x28` | `0x380` | `0x380` | `0x381` | `0x382` | `0x383` | `0x038` |
| Angle 10 | `0x29` | `0x390` | `0x390` | `0x391` | `0x392` | `0x393` | `0x039` |

`ANG_Rate` (base + 3) **được định nghĩa trong DBC nhưng firmware mặc định không phát** — để sẵn cho việc gỡ lỗi mà không phải lên version DBC.

**Hệ trục của module góc: X sang phải xe, Y về đầu xe, Z lên.** Đây **không** phải ISO 8855, nên **Roll quay quanh Y và Pitch quanh X**. Chi tiết và công thức ở `VF_Angle_CAN_Proposal.md` §1.5.

### Bảng chi tiết — module nhiệt–ẩm

Hai message, không phải bốn. `base + 1` và `base + 3` **để trống có chủ ý**, giữ `Status` đúng ở `base + 2` như hai loại trước — người đọc `.blf` quen tìm status của module góc ở `0x302` sẽ tìm status của module này ở `0x402` mà không phải tra bảng.

| Module | `node_id` | base | `TH_Data` | `TH_Status` | Lệnh |
|---|---|---|---|---|---|
| TempHumi 1 | `0x30` | `0x400` | `0x400` | `0x402` | `0x040` |
| TempHumi 2 | `0x31` | `0x410` | `0x410` | `0x412` | `0x041` |
| TempHumi 3 | `0x32` | `0x420` | `0x420` | `0x422` | `0x042` |
| TempHumi 4 | `0x33` | `0x430` | `0x430` | `0x432` | `0x043` |
| TempHumi 5 | `0x34` | `0x440` | `0x440` | `0x442` | `0x044` |
| TempHumi 6 | `0x35` | `0x450` | `0x450` | `0x452` | `0x045` |
| TempHumi 7 | `0x36` | `0x460` | `0x460` | `0x462` | `0x046` |
| TempHumi 8 | `0x37` | `0x470` | `0x470` | `0x472` | `0x047` |
| TempHumi 9 | `0x38` | `0x480` | `0x480` | `0x482` | `0x048` |
| TempHumi 10 | `0x39` | `0x490` | `0x490` | `0x492` | `0x049` |

`TH_Data` mang **`DataAge`** — tuổi của số đo, tính từ lần cảm biến trả lời thành công gần nhất. **Đọc trường đó trước khi tin nhiệt độ và độ ẩm.** Một cảm biến chết vẫn cho ra frame đẹp đẽ với giá trị cuối cùng nó từng đọc được; `VIB_Status.Temperature` của module rung đã đóng băng đúng như vậy suốt nhiều tuần trước 2026-09-26 mà không ai phát hiện.

### Còn trống

`0x00`–`0x0F` (khối dữ liệu `0x100`–`0x1FF`) chưa cấp cho loại nào. Trong mỗi loại, `0x?A`–`0x?F` là 6 chỗ dự phòng nếu một loại cần hơn 10 module. Cấp dải mới thì sửa bảng này trước, rồi mới tới DBC và firmware module — theo thứ tự đó.

**ID nhỏ ưu tiên cao.** Module 1 luôn thắng arbitration trước module 10, và loại `0x1` luôn thắng loại `0x4`. Vị trí đo quan trọng nhất nên nhận `node_id` nhỏ nhất trong loại của nó.

## 2.2 Ngân sách bus — giới hạn vận hành, VIẾT LẠI 2026-09-23

Một frame chuẩn 8 byte ở 500 kbps là 111 bit ≈ **222 µs**, tức **2.22%** tải nếu phát mỗi 10 ms.

### Đơn vị đếm là frame-slot, không phải module

Bảng của v1.1 đếm theo **module** và giả định mỗi module phát **một** frame dữ liệu mỗi chu kỳ. Giả định đó đúng với module rung và **sai với module góc**, vốn phát hai (`ANG_Data` + `ANG_Accel`). Chính v1.1 đã ghi rằng giả định này chưa kiểm chứng cho ba loại còn lại; module góc là loại đầu tiên phá vỡ nó.

Đơn vị đúng:

```
1 frame-slot  =  mot frame du lieu moi 10 ms  =  2.22% tai bus
```

| Loại module | Chu kỳ | Frame/chu kỳ | **Frame-slot** |
|---|---|---|---|
| Rung | 10 ms | 1 | **1** |
| Rung | 5 ms | 1 | **2** |
| Góc | 20 ms | 2 | **1** |
| Góc | 10 ms | 2 | **2** |
| Góc | 50 ms | 2 | **0.4** |
| **Nhiệt–ẩm** | **1000 ms** | **1** | **0.01** |

### Quy tắc vận hành — một dòng, thuộc lòng được

> **Tổng frame-slot trên toàn bus ≤ 20.**

20 slot = 44.4% dữ liệu, cộng status 1 Hz và GPS là **khoảng 45%**. Đó là trần vận hành.

| Đội hình | Slot | Tổng tải |
|---|---|---|
| 10 rung @10 ms + 10 góc @**20 ms** | 10 + 10 = **20** | **45.3%** ✅ |
| 10 góc @10 ms | 20 | **45.1%** ✅ |
| 20 rung @10 ms | 20 | **45.2%** ✅ |
| 10 rung @10 ms + 10 góc @**10 ms** | 10 + 20 = **30** | **67.5%** ❌ |
| 40 rung @10 ms | 40 | **90.1%** ❌ |

**90% là không dùng được.** Ở mức đó độ trễ arbitration bùng lên, node ID lớn bị bỏ đói, và không còn chỗ cho error frame hay retransmission. Ở **67.5%** thì mainboard đã vượt `APP_CAN_LOAD_WARN_PCT = 60` và kêu mỗi giây.

### Quyết định

**Dự án sở hữu 40 module nhưng chỉ cắm 10–20 cái cùng lúc.**

`SET_TX_PERIOD` nhận **`{5, 10, 20, 50}` ms** trên dây, và **mỗi loại module công bố tập con nó hỗ trợ** — từ chối giá trị ngoài tập con bằng `0x02` là hành vi **đúng**, không phải lỗi (`VF_Main_CAN2_Service_Protocol.md` v1.3).

| `module_type` | Tập con | Mặc định | Vì sao |
|---|---|---|---|
| `0x1` Rung | `{5, 10}` | `10` | tóm tắt cửa sổ 26 667 Hz — cửa sổ dài thì nhoè |
| `0x2` Góc | `{10, 20, 50}` | **`20`** | góc là **trạng thái**, không phải bản tóm tắt. Băng thông thân xe < 15 Hz, 50 Hz cho Nyquist 25 Hz. Lấy mẫu trên module vẫn 416 Hz |

Module góc **không** hỗ trợ 5 ms: hai frame ở 5 ms là 8.9% một module — ba cái đã ăn một phần tư bus để đo một thứ có băng thông 15 Hz.

*(v1.1 từng chốt "giữ đúng `5` và `10` ms, `20`/`50` là không cần". Câu đó dựa trên giả định một-frame-mỗi-module và **không còn đúng** — đây chính là "lối 1" mà v1.1 để ngỏ ở cuối mục này.)*

Cắm quá 20 slot cùng lúc không bị phần mềm nào chặn. Nó chỉ đơn giản là không được làm — xem cảnh báo dưới đây.

### Board tự nói khi vượt

Báo cáo `can1` mỗi giây đã in tải bus. Từ phase 7A, tải vượt ngưỡng sẽ kèm một dòng cảnh báo nêu đúng giới hạn ở trên. Không có gì **chặn** được việc cắm quá nhiều module — mainboard chỉ nghe, nó không cấp phát chỗ trên bus — nên cái duy nhất làm được là nói to.

Lý do phải nói to: bus quá tải **không hỏng ra mặt**. Nó chỉ làm node ID lớn trễ dần rồi mất frame, và một file `.blf` thiếu frame trông y hệt một file đủ. Người phân tích sẽ không biết, trừ khi board đã nói lúc đang đo.

### Nếu thật sự cần cả 40 cùng lúc

**Lối 1 đã mở** (v1.2): `SET_TX_PERIOD` nhận `20` và `50` ms. Một module rung ở 20 ms là 0.5 slot, nên 40 module rung ở 20 ms là **20 slot** — vừa đúng trần, không phải sửa gì thêm.

Còn lại một lối, vẫn để ngỏ:

- **Loại `0x3` đã tính, và nó gần như miễn phí.** Cảm biến SHT RS485 chỉ làm mới mỗi 2 giây nên module phát 1 frame/1000 ms = **0,01 frame-slot**. **Mười** module nhiệt–ẩm cộng lại tốn bằng **một phần mười** một module rung. Đây là ví dụ ngược của bài học module góc: lần đó giả định sai làm **tăng** tải gấp đôi, lần này tính ra lại thấy nhẹ hơn trăm lần — cả hai đều là lý do phải **tính** chứ đừng giả định.
- **Loại `0x4` chưa đặc tả.** Bảng frame-slot ở trên cho phép tính trước ngay khi loại đó được thiết kế — nhưng **phải tính**, đừng giả định nó giống bất kỳ loại nào đã có.

## 2.3 Frame lệnh `0x010 + node_id` — đặc tả lần đầu ở v1.2

Dải này được dành chỗ từ v1.0 nhưng **chưa ai định nghĩa byte nào**. Module góc là người dùng đầu tiên, nên định dạng dưới đây cố ý viết **chung cho mọi loại module** — loại `0x3` và `0x4` sẽ kế thừa nguyên vẹn.

> **v1.3: loại `0x3` đã kế thừa nguyên vẹn, và nó là bằng chứng rằng cách viết chung đó đúng.** Module nhiệt–ẩm dùng `PING` (`0x00`) y nguyên, từ chối `ZERO` / `CLEAR_ZERO` / `GYRO_BIAS_CAL` bằng `BAD_CMD` vì chúng vô nghĩa với nó, và định nghĩa đúng **một** lệnh riêng: `0x10 FORCE_READ` — hỏi cảm biến ngay, không chờ hết chu kỳ. Không một byte nào của định dạng chung phải đổi.

DLC luôn **8**.

| Byte | Tên | Nghĩa |
|---|---|---|
| 0 | `Cmd` | mã lệnh |
| 1 | `Seq` | tăng 1 mỗi lệnh mới. Module **bỏ qua** lệnh có `Seq` trùng lệnh vừa nhận |
| 2 | `Arg0` | tham số |
| 3–5 | `Arg1..3` | tham số, bằng 0 khi không dùng |
| 6 | `Magic0` | **`0x5A`** |
| 7 | `Magic1` | **`0xA5`** |

Mã lệnh chung cho mọi loại:

| `Cmd` | Tên | `Arg0` | Tác dụng |
|---|---|---|---|
| `0x00` | `PING` | — | module trả frame status của nó ngay lập tức |
| `0x01` | `ZERO` | mặt nạ trục | đặt trạng thái hiện tại làm mốc |
| `0x02` | `CLEAR_ZERO` | mặt nạ trục | bỏ mốc |
| `0x03` | `GYRO_BIAS_CAL` | — | chạy lại hiệu chuẩn offset con quay |
| `0x04`–`0x0F` | dự trữ | | cho lệnh chung về sau |
| `0x10`+ | riêng theo loại | | mỗi loại tự định nghĩa |

### Ba ràng buộc bắt buộc với mọi loại module

1. **`Seq` chống thực thi hai lần.** CAN tự động phát lại khi lỗi. Một lệnh trạng thái thực thi hai lần giữa phép đo là dữ liệu bị đầu độc.
2. **Hai byte magic phải đúng, nếu không thì bỏ frame.** Một frame rác trùng ID và trùng DLC là khó, nhưng hậu quả — hỏng **im lặng** giữa phép đo — đắt hơn nhiều so với hai byte.
3. **Module echo `Seq` trong frame status của nó, và gửi status ngay khi nhận lệnh.** Đó là ACK duy nhất, và nó phải nằm trong `.blf`: thấy lệnh mà không thấy echo nghĩa là module không nhận.

### Lệnh phải nằm trong log

Mainboard phát frame này lên CAN1 **thật** và ghi nó với **cờ hướng TX**. Đó không phải chi tiết thủ tục: không có nó thì file `.blf` có một chỗ giá trị nhảy mà không lời giải thích, và người phân tích không có cách nào biết đó là lệnh của người vận hành hay là lỗi.

## 3. Message GPS

Mainboard phát 4 frame này ở 5 Hz (cùng nhịp UBX-NAV-PVT), tổng 20 frame/giây ≈ **0.4% tải bus**. Chúng được ghi vào `.blf` với **cờ hướng TX** vì chúng thực sự có trên dây.

| ID | Tên | Chu kỳ | Nội dung |
|---|---|---|---|
| `0x600` | `GPS_Position` | 200 ms | Latitude, Longitude |
| `0x601` | `GPS_Motion` | 200 ms | Altitude, Speed, Heading |
| `0x602` | `GPS_Quality` | 200 ms | FixType, NumSV, HAcc, VAcc, PDOP |
| `0x603` | `GPS_Time` | 200 ms | UnixSeconds, Millisecond, ValidFlags |

Layout bit chi tiết nằm trong bản DBC mới nhất ở `dbc/` (**bản `x` lớn nhất** — `dbc/README.md` nói bản nào đang có hiệu lực). **Đó là nguồn sự thật**, bảng này chỉ là mục lục.

### Mốc thời gian của frame ≠ thời điểm đo

Mốc thời gian của một frame GPS trong `.blf` là **lúc mainboard đưa frame lên bus**, không phải lúc máy thu tính ra vị trí đó. Một CAN trace phải trung thực về thời điểm frame có trên dây, nên đây là đúng — nhưng phải biết để không dùng nhầm.

Đo trên chuyến chạy thật đầu tiên (`20260917_031926.blf`, 5920 epoch):

```
GPS_Time  trừ  mốc thời gian object  =  -0.209 s trung bình
                                        trải -0.353 .. -0.164 s
```

Độ trễ đó gồm: máy thu tính xong rồi bắt đầu phát (~30–50 ms), **truyền serial** (100 byte × 10 bit ÷ baud), và gpsTask lấy mẫu mỗi 10 ms. Ở 9600 riêng phần serial đã là **104 ms**; ở 115200 nó còn **8.7 ms** (xem `APP_GPS_BAUD`, Phase 5G).

**Dùng `GPS_Time` để căn vị trí, đừng dùng mốc thời gian của frame.** Nếu dùng nhầm: ở 36 km/h sai 2 m, ở 100 km/h sai 5.8 m. `GPS_Time` nằm sẵn trong file chính là để không phải chịu sai số này.

### Giá trị "không biết"

Máy thu GNSS trả `0xFFFFFFFF` cho các trường độ chính xác khi nó chưa biết. Chuyển sang CAN, quy ước là:

| Signal | Giá trị nghĩa là "không biết" |
|---|---|
| `HAcc`, `VAcc`, `PDOP` | `0xFFFF` |
| `Speed`, `Heading` | `0xFFFF` |
| `Latitude`, `Longitude`, `Altitude` | `0x7FFFFFFF` |

`HAcc = 0xFFFF` **không phải** 655.35 m. Công cụ phân tích phải lọc nó ra trước khi vẽ, nếu không đồ thị sai số sẽ có những cột 655 m không tồn tại.

Khi `FixType = 0` **hoặc** bit `gnssFixOK` bằng 0, mainboard **không gửi toạ độ thật** — nó gửi `0x7FFFFFFF`, tức 214.7483647 độ. Đó không phải vĩ độ của nơi nào trên trái đất và nằm ngoài dải `[-90|90]` DBC khai báo, nên công cụ đánh dấu được và người nhìn thấy ngay.

**Vì sao không gửi 0:** `0,0` là vịnh Guinea — một toạ độ **thật**, vẽ ra được trên mọi bản đồ. Một bản ghi đầy `0,0` trông y như một chiếc thuyền ngoài khơi Tây Phi.

**Vì sao không im lặng luôn:** một signal ngừng đến sẽ **giữ nguyên giá trị cũ** trong mọi công cụ xem trace. Một vị trí cũ còn khó phát hiện hơn một vị trí bất khả thi.

## 4. Quy tắc

1. **Mọi ID mới lấy từ file này.** Không ai được tự chọn một ID rồi dùng.
2. Thêm ID thì cập nhật cả file này **và** DBC trong cùng một commit. DBC được đánh version và **không sửa tại chỗ**: mỗi lần đổi layout là một file `VF_E-Drive_Data_Logger_v1.<x>.dbc` mới, `x` tăng 1. Quy tắc đầy đủ ở `dbc/README.md`.
3. Dải dự trữ là để dự trữ. Lấn vào đó nghĩa là phải sửa bảng trước.
4. Một `node_id` chỉ thuộc về **một** module vật lý tại một thời điểm. Sổ kiểm kê trên thẻ SD là nơi ghi nhận điều đó.
5. Mỗi loại module có dải `node_id` riêng (mục 2.1). Cấp dải cho loại mới thì sửa mục 2.1 trước, rồi mới tới DBC và firmware module — theo thứ tự đó.
