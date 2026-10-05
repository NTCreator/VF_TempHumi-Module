# contracts/ — BẢN SAO, CHỈ ĐỌC

Bốn file trong thư mục này **thuộc về repo `VF_Main`**. Đây là bản sao.

```
VF_Main/dbc/VF_E-Drive_Data_Logger_v1.<x>.dbc     ->  contracts/  (ban x lon nhat)
VF_Main/docs/VF_Main_CAN_ID_Map.md                ->  contracts/
VF_Main/docs/VF_Main_CAN2_Service_Protocol.md     ->  contracts/
VF_Main/docs/VF_Angle/VF_Angle_CAN_Proposal.md    ->  contracts/
```

**DBC có đánh version và tên file đổi theo.** Bên `VF_Main`, mỗi lần sửa layout là một file `v1.<x+1>` mới và bản cũ được giữ lại (`VF_Main/dbc/README.md`). Ở đây thì ngược lại: `contracts/` chỉ giữ **đúng một** bản — bản đang có hiệu lực. Chép bản mới sang thì **xoá bản cũ đi**, nếu không `check_contracts.sh` báo tập file đã đổi. Đó là hành vi đúng, không phải phiền toái: nó bắt người ta nhìn.

Tránh dấu cách trong tên file DBC: `check_contracts.sh` chạy `$SHA $FILES` không đóng ngoặc, nên một tên có dấu cách sẽ bị tách thành nhiều tham số.

## Vì sao `VF_Angle_CAN_Proposal.md` cũng được băm

Nó trông như tài liệu thiết kế, nhưng nó **chứa layout bit của từng message** — vị trí và thang của `Roll`, `Pitch`, `Yaw`, định dạng frame lệnh, ý nghĩa từng bit cờ. Sửa một dòng trong đó là sửa hợp đồng, y như sửa DBC.

Nó cũng là nơi duy nhất ghi **hệ trục**: X sang phải xe, Y về đầu xe, Z lên — **không phải ISO 8855**, nên Roll quay quanh Y và Pitch quanh X. Một thay đổi âm thầm ở đó lật ngược mọi đồ thị mà không ai phát hiện được từ trong file `.blf`.

## Không sửa file ở đây

Cần đổi thì sửa ở `VF_Main` rồi chép sang. `tools/check_contracts.sh` so SHA-256 **và tập tên file**, rồi **fail build** khi lệch.

```sh
tools/check_contracts.sh            # kiem, exit 1 khi lech
tools/check_contracts.sh --update   # nhan cac file hien tai lam moc moi
```

`--update` là một **hành động có chủ đích**: nó nghĩa là "tôi đã đọc thay đổi và firmware đã xử lý được rồi". Không bao giờ chạy nó chỉ để làm một build đỏ thành xanh.

## Trạng thái hiện tại

| File | Version | Duyệt |
|---|---|---|
| `VF_E-Drive_Data_Logger_v1.2.dbc` | v1.2 | 2026-09-23 |
| `VF_Main_CAN_ID_Map.md` | v1.2 | 2026-09-23 |
| `VF_Main_CAN2_Service_Protocol.md` | v1.3 | 2026-09-23 |
| `VF_Angle_CAN_Proposal.md` | — | 2026-09-23 |

**Dải `node_id` của repo này là `0x20`–`0x29`** (loại `0x02`). Khối dữ liệu `0x300`–`0x39F`, lệnh `0x030`–`0x039`.
