# Giao thức dịch vụ CAN2 — mainboard ↔ bootloader của module

**Trạng thái: v1.5 — ĐÃ DUYỆT 2026-10-05.** Đây là **hợp đồng giữa hai firmware**: mainboard (dự án này) và bootloader của module (dự án khác). Đổi nó nghĩa là nạp lại toàn bộ module đã xuất xưởng.

Lệnh nạp firmware `0x40`+ vẫn **chưa đặc tả** và cố tình để lại cho một vòng duyệt riêng — xem mục 7. Phần cấu hình (`0x01`–`0x30`) thì đã đủ để viết code hai bên.

> **v1.5 so với v1.4 — không đổi một byte nào trên dây.** Đặt tên loại `0x03` là **module nhiệt–ẩm** và công bố tập `SET_TX_PERIOD` của nó là **rỗng** (§4.2): chu kỳ cố định 1000 ms vì cảm biến chỉ làm mới mỗi 2 giây. Không có lệnh mới, không có trường mới, không có mã lỗi mới — `0x02 BAD_PARAM` vốn đã là câu trả lời đúng. Firmware module rung và module góc **không phải sửa một dòng nào**. Đi kèm `VF_Main_CAN_ID_Map.md` v1.3 và `dbc/VF_E-Drive_Data_Logger_v1.5.dbc`.
>
> **v1.4 so với v1.3 — không đổi một byte nào trên dây.** Hai chỗ, cả hai đến từ buổi chạy thật đầu tiên trên mainboard (`VF_Angle_Phase6a2_Report.md` §11.2, 2026-09-23):
>
> - **§5.5 viết lại.** Mô tả cũ về cuộc đua **sai tiền đề**: nó giả định module khởi động cùng lúc với lệnh `mod open`. J2 cấp 5 V **thường trực**, nên module khởi động lúc chạm connector và một module **đã cấu hình** rời bootloader trước khi người vận hành gõ xong. Triệu chứng đo được cũng khác với điều v1.3 dự đoán — xem §5.5.
> - **Bất biến 5 thêm một câu**: config không hợp lệ ⇒ bootloader **ở lại vĩnh viễn**, giống hệt khi không có ứng dụng hợp lệ.

> **v1.3 so với v1.2 — cộng thêm, không phá vỡ.** Loại `0x02` là **module đo góc**. `SET_TX_PERIOD` nhận thêm `20` và `50` ms, và mỗi loại module công bố **tập con** nó hỗ trợ (mục 4.2). Thêm lệnh **`0x14 SET_GYRO_FS`**. `SET_FULL_SCALE` được nói rõ là dành cho **gia tốc kế**.
>
> **Firmware module rung không phải sửa một dòng nào:** nó đang từ chối `20`/`50` bằng `0x02`, và theo cách diễn đạt mới thì đó vẫn là đúng hợp đồng; `0x14` nó trả `0x01 UNKNOWN_CMD` (không phải `0x02`: nó không có lệnh đó, chứ không phải tham số sai) — cũng đúng.
>
> **v1.2 so với v1.1:** thêm bất biến 5 — cửa sổ nghe ≥ 1000 ms của bootloader (mục 5). Không đổi lệnh nào, không đổi byte nào trên dây.
>
> **v1.1 so với v1.0:** dự án chốt 4 loại module × 10, nên `module_type` đã có bảng (mục 4.1), `node_id` mở rộng lên `0x00`–`0x4F`, và module rung chuyển sang dải `0x10`–`0x19`. Đi cùng `VF_Main_CAN_ID_Map.md` v1.1.

## 1. Bối cảnh

Module chỉ có 4 chân — `5V`, `GND`, `CAN-H`, `CAN-L` — nên không có chỗ cho DIP switch hay strap địa chỉ. Message ID được gán qua bus.

Việc đó **không** xảy ra trên CAN1 (bus đo). Nó xảy ra trên **CAN2 qua connector J2**, là đường điểm-điểm tới đúng một module:

```
J2 [1] +5V    J2 [2] CAN2_H    J2 [3] GND    J2 [4] CAN2_L
```

J2 cấp điện cho module, nên cấu hình một module chỉ cần mainboard, không cần nguồn bench.

**Quy trình:** rút module khỏi CAN1 → cắm vào J2 → cấu hình trên màn hình TFT → rút ra → cắm lại lên CAN1.

**Tuyệt đối không cắm module vào J2 khi nó còn nối CAN1**: hai nguồn 5V đối đầu và hai điện trở đầu cuối trên cùng một cặp dây.

## 2. Vì sao giao thức này đơn giản hơn nhiều so với cấu hình qua CAN1

Đầu kia có **đúng một thiết bị**. Hệ quả:

- Không cần định danh bằng UID 96 bit để chọn node — cứ nói là nó nghe.
- Không cần lệnh quét bus, không cần phát hiện trùng ID, không cần CANopen LSS.
- Không có tranh chấp arbitration với ai.

UID vẫn được đọc, nhưng để **kiểm kê và dán nhãn**, không phải để định địa chỉ.

## 3. Tham số lớp vật lý

| | |
|---|---|
| Tốc độ | 500 kbps (giống CAN1, dùng chung tính toán bit timing) |
| ID | 11 bit chuẩn |
| Request (mainboard → module) | `0x7E0` |
| Response (module → mainboard) | `0x7E8` |
| Độ dài | luôn 8 byte, phần thừa điền `0x00` |
| Đầu cuối | R16 trên mainboard. Dây ngắn tới một module thì một đầu cuối thường đủ ở 500 kbps — **đo một lần, đừng giả định** |
| Timeout | module phải trả lời trong 50 ms; mainboard thử lại tối đa 3 lần. **Ngoại lệ: `HELLO` đầu tiên của một phiên** được hỏi lặp lại tới 10 s — xem §5.5 |

`0x7E0`/`0x7E8` mượn quy ước UDS cho dễ nhận ra khi soi bằng công cụ. Chúng **chỉ tồn tại trên CAN2** nên không chiếm chỗ trong bảng ID của CAN1.

## 4. Khung lệnh

Byte 0 của request là mã lệnh. Byte 0 của response lặp lại mã lệnh, byte 1 là mã trạng thái.

```
Request :  [0] cmd   [1..7] tham số
Response:  [0] cmd   [1] status   [2..7] dữ liệu
```

### Mã trạng thái

| Giá trị | Nghĩa |
|---|---|
| `0x00` | OK |
| `0x01` | lệnh không hiểu |
| `0x02` | tham số sai |
| `0x03` | lỗi ghi flash |
| `0x04` | không cho phép ở trạng thái hiện tại |

### Lệnh

| Mã | Tên | Request | Response |
|---|---|---|---|
| `0x01` | `HELLO` | — | `[2]` proto_ver, `[3]` module_type, `[4]` node_id, `[5..7]` fw major/minor/patch |
| `0x02` | `GET_UID_LO` | — | `[2..7]` UID byte 0–5 |
| `0x03` | `GET_UID_HI` | — | `[2..7]` UID byte 6–11 |
| `0x10` | `SET_NODE_ID` | `[1]` node_id (`0x00`–`0x4F`) | — |
| `0x11` | `SET_MODULE_TYPE` | `[1]` type (`0x01`–`0x04`) | — |
| `0x12` | `SET_TX_PERIOD` | `[1]` ms: `5`, `10`, `20` hoặc `50` — **tuỳ loại**, xem 4.2 | — |
| `0x13` | `SET_FULL_SCALE` | `[1]` **gia tốc kế**, đơn vị g: `2`, `4`, `8` hoặc `16` | — |
| `0x14` | `SET_GYRO_FS` | `[1]` **con quay** = dps ÷ 125: `1`, `2`, `4`, `8`, `16`, `32` | — |
| `0x20` | `COMMIT` | — | ghi config vào flash |
| `0x30` | `RESET` | — | trả lời **trước** rồi mới reset |
| `0x40`+ | nạp firmware | *(chưa đặc tả — làm cùng lúc với phần flash)* | |

**Mọi lệnh `SET_*` (`0x10`–`0x14`) chỉ đổi giá trị trong RAM. `COMMIT` mới ghi xuống flash.** Tách ra để đặt hết mọi tham số rồi ghi một lần, và để hủy giữa chừng không để lại gì.

### `0x14 SET_GYRO_FS` — vì sao mã hoá là `dps ÷ 125`

| Byte | Full scale |
|---|---|
| `1` | ±125 dps |
| `2` | ±250 dps |
| `4` | ±500 dps |
| `8` | ±1000 dps |
| `16` | ±2000 dps |
| `32` | ±4000 dps |

Không dùng chỉ số `0`–`5`. Với chỉ số liên tiếp, một bit lật trong truyền dẫn biến một giá trị hợp lệ thành **một giá trị hợp lệ khác**, và module nhận nó không chút nghi ngờ. Với bậc luỹ thừa hai, đa số byte sai rơi vào khoảng trống và bị từ chối bằng `0x02`.

Cùng lý lẽ với `SET_FULL_SCALE` (`2/4/8/16` chứ không phải `0/1/2/3`).

**Lệnh này chỉ áp dụng cho loại có con quay.** Module loại `0x01` (rung) trả **`0x01 UNKNOWN_CMD`** — đúng hợp đồng, không phải lỗi.

Phân biệt hai mã, vì chúng nói hai chuyện khác nhau và màn hình phải hiện khác nhau:

| Mã | Nghĩa | Ví dụ |
|---|---|---|
| `0x01` `UNKNOWN_CMD` | **Loại module này không có lệnh đó** | gửi `0x14` tới module rung |
| `0x02` `BAD_PARAM` | **Có lệnh, nhưng giá trị ngoài tập con nó hỗ trợ** | gửi `SET_TX_PERIOD = 20` tới module rung |

Module **kiểm tham số ngay khi nhận `SET_*`**, không đợi tới `COMMIT`: một giá trị ngoài dải bị từ chối bằng status `0x02` tại chỗ, để màn hình báo lỗi ngay chứ không phải sau khi người vận hành đã đặt xong cả bốn trường.

`node_id` trên dây là `0x00`–`0x4F`; `0xFF` nghĩa là chưa cấu hình. `0x50` trở lên **không bao giờ hợp lệ**: base của nó rơi vào dải GPS (`VF_Main_CAN_ID_Map.md` mục 1).

## 4.1 `module_type` và dải `node_id` của nó

Dự án có 40 sensor, chia thành 4 loại module, mỗi loại 10. **Nibble cao của `node_id` chính là `module_type`:**

| `module_type` | Loại | `node_id` hợp lệ | Khối dữ liệu trên CAN1 |
|---|---|---|---|
| `0x01` | Rung (`VF_Vibration_Module`) | `0x10`–`0x19` | `0x200`–`0x29F` |
| `0x02` | **Góc** (`VF_Angle_Module`) | `0x20`–`0x29` | `0x300`–`0x39F` |
| `0x03` | *chưa đặt tên* | `0x30`–`0x39` | `0x400`–`0x49F` |
| `0x04` | *chưa đặt tên* | `0x40`–`0x49` | `0x500`–`0x59F` |

Module **phải tự kiểm** và trả `0x02` nếu không thoả:

```c
ok = ((node_id >> 4) == module_type) && ((node_id & 0x0Fu) <= 0x09u);
```

Phép kiểm này chạy lúc nhận `SET_NODE_ID` **và** lúc nhận `SET_MODULE_TYPE` — đổi một trong hai có thể phá vỡ quan hệ giữa chúng. Nếu cặp đang giữ trong RAM không nhất quán, `COMMIT` trả `0x02` và không ghi gì.

Vì sao từ chối **ồn ào** thay vì nhận bừa: một module rung mang nhầm `node_id = 0x25` sẽ phát ở `0x350`/`0x351`. Đó là chỗ của **module loại 2** — nên nó vừa phát ra thứ DBC mô tả sai, vừa có thể đụng trực tiếp với một module loại 2 thật. Trùng ID đẩy cả hai node vào bus-off (mục 5.3), chứ không chỉ làm rối log.

`VIB_Status` lặp lại `node_id` trong payload đúng để chuyện này lộ ra **từ chính file `.blf`**, không cần cắm lại module mới biết.

## 4.2 Tập tham số hợp lệ theo `module_type`

Giá trị hợp lệ **trên dây** là tập hợp lớn nhất. Mỗi loại module công bố **tập con** nó thực sự hỗ trợ.

| `module_type` | `SET_TX_PERIOD` | mặc định | `SET_FULL_SCALE` (g) | `SET_GYRO_FS` |
|---|---|---|---|---|
| `0x01` Rung | `{5, 10}` | `10` | `{2, 4, 8, 16}` | **không hỗ trợ** |
| `0x02` Góc | `{10, 20, 50}` | **`20`** | `{2, 4, 8, 16}`, mặc định `4` | `{1,2,4,8,16,32}`, mặc định `2` (±250 dps) |
| `0x03` Nhiệt–ẩm | **tập rỗng** — cố định 1000 ms | — | **không hỗ trợ** | **không hỗ trợ** |
| `0x04` | *chưa đặc tả* | | | |

### Loại `0x03` có tập `SET_TX_PERIOD` RỖNG — và đó là câu trả lời, không phải chỗ trống

Mọi giá trị của `SET_TX_PERIOD` gửi tới module nhiệt–ẩm đều bị từ chối bằng `0x02 BAD_PARAM`. Chu kỳ phát của nó **cố định 1000 ms**, và mainboard không được hiển thị nút chọn chu kỳ cho loại này.

Lý do không phải là payload một byte không chứa nổi 1000:

> **Cảm biến SHT RS485 chỉ làm mới giá trị mỗi 2 giây** (datasheet, "Thời gian làm mới"). Mọi chu kỳ phát có nghĩa đều nằm trong khoảng 1–2 s, và khác nhau giữa chúng không thay đổi được gì trong phân tích. Cho người vận hành bốn nút để chọn giữa những thứ như nhau là mời họ tin rằng lựa chọn đó quan trọng.

Nếu về sau có một loại module cần chu kỳ chậm **mà có lý do để chọn**, đường nâng cấp đúng là mở rộng payload `SET_TX_PERIOD` thành **1 hoặc 2 byte** (uint16 LE ms, 1 byte giữ nguyên nghĩa cũ) — cộng thêm, không phá vỡ. Đã cân nhắc và **cố tình chưa làm**: thêm một dạng payload cho một người dùng chưa tồn tại là thêm một nhánh chưa ai chạy thử.

**Từ chối một giá trị ngoài tập con của mình bằng `0x02` là hành vi ĐÚNG, không phải lỗi.** Mainboard phải hiểu `0x02` ở đây là "loại module này không làm được", chứ không phải "giao thức sai" — và nói đúng như vậy trên màn hình.

### Vì sao mỗi loại một tập con

Ngân sách bus tính theo **frame-slot**, không theo module (`VF_Main_CAN_ID_Map.md` mục 2.2). Module góc phát **hai** frame mỗi chu kỳ, nên nó ở 5 ms là **8.9% bus một cái** — ba module đã ăn một phần tư bus để đo một thứ có băng thông 15 Hz.

Ngược lại module rung **cần** 10 ms: nó tóm tắt một cửa sổ 26 667 Hz, và cửa sổ càng dài thì bản tóm tắt càng nhoè. Góc thì không — góc là một **trạng thái**, không phải bản tóm tắt, nên phát thưa hơn không mất thông tin.

Một tập giá trị chung cho mọi loại sẽ hoặc cấm module rung làm điều nó cần, hoặc cho phép module góc làm điều nó không nên. Tập con theo loại là cách duy nhất đúng cho cả hai.

## 5. Bất biến bắt buộc của firmware module

Mainboard phụ thuộc vào cả năm điều dưới đây. Thiếu một là hỏng.

1. **Giao thức nằm trong bootloader, không nằm trong ứng dụng.** Module có ứng dụng hỏng hoặc flash ứng dụng trắng vẫn phải trả lời được — đó chính là lúc cần cấu hình nhất. Một giao thức, một code path, luôn với tới được.

2. **Config lưu kèm magic và CRC.**

3. **Config không hợp lệ → module IM LẶNG trên CAN1**, chỉ trả lời trên CAN2. Không bao giờ đoán một ID rồi phát.

   Đây không phải chuyện gọn gàng. Hai node cùng ID phát cùng lúc: qua được arbitration vì ID giống nhau, rồi lệch nhau ở phần data → **bit error** → error frame → phát lại liên tục → **cả hai có thể vào bus-off**. Trùng ID làm sập bus chứ không chỉ làm rối log. Mất điện giữa lúc ghi flash phải rơi vào trạng thái im lặng, không phải trạng thái đoán mò.

4. **`RESET` trả lời trước khi reset.** Nếu không, mainboard không phân biệt được "đã reset" với "module chết".

5. **Cửa sổ nghe ≥ 1000 ms sau khi cấp nguồn.** Bootloader phải nghe `0x7E0` **ít nhất 1000 ms** trước khi nhảy sang ứng dụng. Nghe thấy một request trong cửa sổ đó thì ở lại bootloader và không bao giờ nhảy nữa cho tới lần cấp nguồn sau.

   **Hai trường hợp bootloader KHÔNG áp cửa sổ mà ở lại vĩnh viễn:**

   | Trường hợp | Vì sao |
   |---|---|
   | Không có ứng dụng hợp lệ | flash trắng đọc ra `0xFF`, không bao giờ qua được phép kiểm ứng dụng. Đây là đường cứu |
   | **Config không hợp lệ** *(v1.4)* | module này **không đo được gì cả** — config sai thì ứng dụng phải im lặng trên CAN1 (bất biến 3). Nhảy sang một ứng dụng chỉ để nó nằm im là vô nghĩa, và nó làm module trở nên khó với tới đúng lúc cần với tới nhất |

   Phép kiểm config mà bootloader dùng phải là **đúng cái hàm ứng dụng dùng** để quyết định im lặng. Hai phép kiểm khác nhau cho ra một module vừa không đo vừa không cấu hình được.

   > ⚠️ **Hệ quả:** bootloader trở thành trọng tài của định dạng config. Đổi `APP_CFG_VERSION` mà chỉ cập nhật ứng dụng sẽ làm bootloader cũ coi config mới là sai và ở lại mãi — module vẫn cứu được qua CAN2 nhưng không đo. **Đổi định dạng config = cập nhật bootloader cùng lúc.**

### 5.5 Cuộc đua — viết lại ở v1.4 sau buổi chạy thật đầu tiên

Bản v1.3 mô tả cuộc đua thế này: mainboard đợi 200 ms rồi gửi `HELLO`, thử 3 lần × 50 ms, xong trước ~350 ms, biên gần 3 lần so với cửa sổ 1000 ms.

**Tiền đề đó sai.** Nó giả định module khởi động cùng lúc mainboard bắt đầu hỏi. Thực tế:

> **J2 cấp 5 V thường trực, không ngắt được bằng phần mềm** (mục 7). Module khởi động **lúc chạm connector**, không phải lúc người vận hành gõ `mod open`. Một module **đã cấu hình** rời bootloader khoảng một giây sau đó — rất lâu trước khi ai gõ xong một dòng lệnh.

Nên cuộc đua thật không phải "mainboard có kịp trong một lần khởi động không", mà là "**module đã khởi động từ lúc nào**". Không có biên an toàn nào cứu được điều đó.

#### Triệu chứng đo được — v1.3 đoán sai chỗ này

v1.3 nói triệu chứng là `nothing acknowledged the request`, trùng với chuỗi một connector chưa cắm sinh ra, nên người vận hành sẽ đi kiểm tra dây oan.

Thực tế **tốt hơn thế**, và hai chuỗi phân biệt được:

| Mainboard báo | Nghĩa là |
|---|---|
| `nothing acknowledged the request` | **không có node nào ở đầu kia.** Connector, dây, hoặc module không có điện |
| **`no answer from the module`** | **module CÓ ở đó và đang chạy ứng dụng.** Nó ACK mọi frame đúng khuôn trên CAN2 — chung một bxCAN — nhưng giao thức nằm trong bootloader nên không ai trả lời `0x7E0` |

Module chạy ứng dụng vẫn ACK, vì nó là một node thật trên bus. Đó là thứ biến một chẩn đoán nhập nhằng thành hai chẩn đoán rõ ràng.

#### Mainboard làm gì với điều đó

`mod open` hỏi lặp lại trong `APP_CAN2_OPEN_WAIT_MS` = **10 000 ms**, không phải 3 lần × 50 ms, và in một dòng nhắc mỗi 2 giây.

Module là bên có hạn 1 giây; mainboard là bên **chờ được**. Nên quy trình vận hành là:

| Module | Thao tác |
|---|---|
| **Đã cấu hình** | `mod open` **rồi** bấm nút reset trên module (hoặc rút–cắm) trong 10 giây |
| **Chưa cấu hình** | `mod open` lúc nào cũng được — bootloader ở lại vĩnh viễn (bất biến 5) |

Trường hợp thứ hai là đường cứu, và nó **không cần canh giờ gì cả** — đúng như nó phải thế.

## 6. Phía mainboard phải làm

- Hiện **UID** (6 hex cuối là đủ) trên màn hình lúc cấu hình. Module giống hệt nhau về hình dáng; không có UID thì sau khi cấu hình 5 cái sẽ không còn cách nào biết cái nào là cái nào.
- Ghi **sổ kiểm kê** xuống thẻ SD, nối thêm một dòng mỗi lần `COMMIT` thành công. Cái nhãn dán trên vỏ không được là nơi duy nhất giữ thông tin.

  ```
  {ngày giờ, UID 96 bit, module_type, node_id, tx_period_ms, full_scale_g, kết quả}
  ```

  **Ghi cả bốn tham số, không chỉ `node_id` và `module_type`.** Người vận hành đặt bốn thứ, nên sổ phải trả lời được cả bốn — nếu không thì câu hỏi "module 3 lúc đó đang để full scale bao nhiêu" chỉ còn cách đoán. `full_scale` và `tx_period` có xuất hiện trong `VIB_Status` và trong nhịp frame, nhưng chỉ khi module **đang chạy trên bus**; sổ kiểm kê phải trả lời được cả khi module nằm trong hộp.

  Ghi cả lần `COMMIT` **thất bại** (kèm mã trạng thái). Một module bị bỏ dở giữa chừng là thứ cần biết nhất, và đó đúng là lúc không có gì khác ghi lại nó.
- **Cấm cấu hình trong lúc đang ghi log.** Mainboard là thiết bị đo; nó không được vừa đo vừa sửa thứ nó đo. Về vật lý thì module đã rút khỏi CAN1 rồi, nhưng luật này chặn nốt phần con người.
- Bật EN_CAN2 (LOW = Normal) chỉ trong lúc cấu hình, còn lại giữ HIGH (Standby).

## 7. Chưa quyết

- **Tên của loại `0x03`, `0x04`**: đã giữ chỗ trong bảng ở mục 4.1 nhưng chưa biết là sensor gì. `0x01` = rung và `0x02` = góc thì **đã chốt**.

  Khi đặc tả chúng, việc **bắt buộc** là tính frame-slot trước khi chọn chu kỳ (mục 4.2). Đừng giả định chúng giống module rung — giả định "một frame mỗi module" của v1.1 sống được đúng một ngày trước khi module góc phá vỡ nó.
- **Lệnh nạp firmware `0x40`+**: vẫn chưa đặc tả, và **cố tình tách khỏi vòng duyệt này**. Phần cấu hình `0x01`–`0x30` đã đủ để cả hai bên viết code; phần nạp firmware cần thiết kế riêng (đánh số khối, CRC toàn ảnh, xử lý mất điện giữa chừng) và một vòng duyệt riêng.

  Cho tới khi có nó, firmware ứng dụng nạp bằng **SWD**. Khâu sản xuất đằng nào cũng phải cắm SWD một lần để nạp bootloader, nên nạp luôn cả ứng dụng trong cùng lần đó không tốn thêm thao tác. Cái giá chỉ phát sinh khi cần cập nhật firmware cho module **đã xuất xưởng**.
- **Nguồn 5V trên J2 không ngắt được bằng phần mềm** — netlist cho thấy `+5` nối thẳng. Cắm rút khi đang có điện sẽ mòn tiếp điểm. Nếu muốn ngắt được thì phải sửa phần cứng; còn không thì quy trình là tắt nguồn mainboard trước khi cắm.
