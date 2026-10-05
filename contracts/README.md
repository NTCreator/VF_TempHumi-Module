# contracts/ — BẢN SAO, CHỈ ĐỌC

Bốn file hợp đồng trong thư mục này là **bản sao**. File này (`README.md`) và `SHA256SUMS` thì thuộc về repo này: script không băm `README.md`, và `SHA256SUMS` chính là bảng băm.

```
VF_Main/dbc/VF_E-Drive_Data_Logger_v1.<x>.dbc     ->  contracts/  (ban x lon nhat)
VF_Main/docs/VF_Main_CAN_ID_Map.md                ->  contracts/
VF_Main/docs/VF_Main_CAN2_Service_Protocol.md     ->  contracts/
docs/VF_TH_CAN_Proposal.md (repo nay, xem duoi)   ->  contracts/
```

**DBC có đánh version và tên file đổi theo.** Bên `VF_Main`, mỗi lần sửa layout là một file `v1.<x+1>` mới và bản cũ được giữ lại (`VF_Main/dbc/README.md`). Ở đây thì ngược lại: `contracts/` chỉ giữ **đúng một** bản — bản đang có hiệu lực. Chép bản mới sang thì **xoá bản cũ đi**, nếu không `check_contracts.sh` báo tập file đã đổi. Đó là hành vi đúng, không phải phiền toái: nó bắt người ta nhìn.

Tránh dấu cách trong tên file DBC: `check_contracts.sh` chạy `$SHA $FILES` không đóng ngoặc, nên một tên có dấu cách sẽ bị tách thành nhiều tham số.

## Vì sao `VF_TH_CAN_Proposal.md` cũng được băm

Nó trông như tài liệu thiết kế, nhưng nó **chứa layout bit của từng message** — vị trí và thang của `Temperature`, `Humidity`, **`DataAge`**, ý nghĩa từng bit `Flags`, và quyết định "chu kỳ cố định 1000 ms" (phương án A). Sửa một dòng trong đó là sửa hợp đồng, y như sửa DBC.

Riêng `DataAge`: nó là lý do message `TH_Data` tồn tại (CLAUDE.md §1.1). Một thay đổi âm thầm về đơn vị hay giá trị `0xFFFF` ("chưa đọc được lần nào") làm mọi phân tích độ cũ của số đo sai mà không ai phát hiện được từ trong file `.blf`.

### ⚠️ Nhà chính thức của file này chưa có

Khác với `VF_Angle_CAN_Proposal.md` (nằm ở `VF_Main/docs/VF_Angle/`), **`VF_Main` hiện không có bản nào của `VF_TH_CAN_Proposal.md`** (kiểm 2026-10-05). Bản gốc thực tế là `docs/VF_TH_CAN_Proposal.md` của repo này, giống từng byte với bản ở đây.

Việc còn mở: đưa nó vào `VF_Main/docs/VF_TH/` như module góc, rồi coi bản đó là gốc. Cho tới lúc đó, **sửa `docs/VF_TH_CAN_Proposal.md` mà không sửa bản ở đây là được phép về mặt kỹ thuật nhưng sai về quy tắc** — `check_contracts.sh` chỉ canh `contracts/`.

## Không sửa file ở đây

Cần đổi thì sửa ở `VF_Main` rồi chép sang. `tools/check_contracts.sh` so SHA-256 **và tập tên file**, rồi **fail build** khi lệch (qua `makefile.init` ở gốc repo).

```sh
tools/check_contracts.sh            # kiem, exit 1 khi lech
tools/check_contracts.sh --update   # nhan cac file hien tai lam moc moi
```

`--update` là một **hành động có chủ đích**: nó nghĩa là "tôi đã đọc thay đổi và firmware đã xử lý được rồi". Không bao giờ chạy nó chỉ để làm một build đỏ thành xanh.

## Trạng thái hiện tại

| File | Version | Duyệt |
|---|---|---|
| `VF_E-Drive_Data_Logger_v1.5.dbc` | v1.5 | 2026-10-05 |
| `VF_Main_CAN_ID_Map.md` | v1.3 | 2026-10-05 |
| `VF_Main_CAN2_Service_Protocol.md` | v1.5 | 2026-10-05 |
| `VF_TH_CAN_Proposal.md` | — (phương án A) | 2026-10-05 |

Ba file đầu khớp hash với `VF_Main` (kiểm 2026-10-05).

**Dải `node_id` của repo này là `0x30`–`0x39`** (loại `0x3`). Khối dữ liệu `0x400`–`0x49F`, lệnh `0x040`–`0x049`.

## Hai lỗi chữ đã biết trong hợp đồng — không ảnh hưởng firmware

Ghi ở đây vì không được sửa tại chỗ; sửa ở nhà chính thức trong lần bump sau.

| File | Chỗ | Sai | Đúng |
|---|---|---|---|
| `VF_TH_CAN_Proposal.md` | tiêu đề §5.3 | `TH_Cmd — 0x040 + node_id` | `0x010 + node_id` (§2, ID Map §2.1, DBC `BO_ 64 TH_Cmd_01`) |
| `VF_Main_CAN2_Service_Protocol.md` | bảng §4.1, dòng `0x03` | *chưa đặt tên* | Nhiệt–ẩm — §4.2 và ghi chú v1.5 của chính file đó đã đặt tên |

Firmware dùng công thức của §2, và `_Static_assert` trong `Core/Inc/app/app_config.h` khoá nó vào bảng `0x040`–`0x049`.
