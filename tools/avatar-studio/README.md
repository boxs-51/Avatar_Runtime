# Avatar Studio

Giao diện desktop để quan sát và chỉnh model local theo thời gian thực.
Từ thư mục repo, chạy `launch_interactive_viewer.bat` hoặc:

```powershell
python scripts/avatar_studio.py
python scripts/avatar_studio.py --model "E:\models\sample.model3.json"
```

Nếu có đúng một model trong `local-assets/`, Studio tự chọn model đó; nếu không,
Studio mở hộp chọn file. `MOC3_VIEWER_PROFILE` hiện có vẫn được ưu tiên. Nút
**Open model** thay model trong cửa sổ; model lỗi giữ nguyên model đang chạy.

- Chín biểu cảm chuyển tiếp mượt; tự đổi biểu cảm, chớp mắt, thở, nhìn theo
  chuột, nói mô phỏng, điều chỉnh tốc độ/cường độ/chuyển động gió.
- Physics3 liên kết với model chạy bằng retained solver hiện có. Trạng thái
  thiếu hoặc lỗi physics hiện rõ; không tự thay bằng spring giả.
- Kéo chuột trái để điều khiển đầu/tay/thân; chuột phải/giữa để pan; cuộn để zoom.
  Fit/Face đặt camera, Reset khôi phục scene và các giá trị chỉnh tay.
- Inspector tìm theo ID, chỉnh và ghim tham số, chỉnh độ hiện của part/layer.
  Chuột phải trên hàng đã ghim để bỏ ghim. Clear manual overrides khôi phục
  tham số và opacity gốc. Tham số model không có sẽ không được tự tạo.
- Space pause/play; Step tiến một frame mô phỏng; P bật/tắt physics; 1–9 chọn
  biểu cảm; R fit camera; F12 hoặc Capture lưu PNG viewport.
- Bốn nền, grid, particles, biểu đồ thời gian frame; Save/Load preset giữ các
  tùy chỉnh và override. Preset sai model hoặc hỏng bị từ chối.

Preset, `state.json` và PNG chỉ lưu trong **`build/studio-user/`**, được Git bỏ
qua. Preset giữ zoom nhưng không lưu vị trí pan. Capture/F12 ghi đè
`viewport.png`; hãy sao chép ảnh cần giữ trước lần chụp kế tiếp. Các điều khiển
chu kỳ bổ sung xuất hiện khi cửa sổ đủ cao. Giao diện dùng tiếng Anh.

`--legacy` mở viewer cũ. `--build` yêu cầu build tăng dần. Studio tự build khi
chưa có binary hoặc source Studio/runtime mới hơn binary. Cần Windows x64,
CMake và Visual Studio C++; dùng source Purism Core/raylib/utf8proc đã có trong
`build/`. Không tải dependency hay SDK mới. Một checkout mới chưa có các source
local này cần chuẩn bị dependency trước; launcher sẽ báo lỗi rõ.

```powershell
python scripts/avatar_studio.py --smoke --frames 1800
$env:ARS_STUDIO_LIVE='1'
python -m unittest discover -s tests/moc3 -p test_avatar_studio_live.py -v
```

Smoke mở cửa sổ thật, kiểm tra physics ở biên tốc độ, pause, preset, resize và
lưu ảnh vào thư mục riêng. Bài test GUI gửi phím/chuột chỉ khi đúng cửa sổ do nó
mở đang ở foreground, rồi đóng process đó. Khi chạy bài này, để chuột/bàn phím
rảnh cho test. Chạy thường không gửi input tự động.

Đây là viewer chẩn đoán **trusted-local** dùng Purism Core/sample OpenGL renderer
(MIT, Sakura Motion Project) và retained Physics3 của repo. Chỉ mở model tin cậy.
Biểu cảm, nói và gió là procedural controls, không phải phát motion/expression
do tác giả model cung cấp hay nhận âm thanh từ micro. Kết quả không thay chứng
cứ ARS host/D3D11, không chứng minh tương đương Cubism và không đóng gate sản phẩm.
