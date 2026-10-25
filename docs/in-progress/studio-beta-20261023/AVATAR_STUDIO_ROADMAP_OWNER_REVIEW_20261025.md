# AVATAR STUDIO — LỘ TRÌNH TRIỂN KHAI TỔNG THỂ

**Mã tài liệu:** AS-ROADMAP-001
**Phiên bản:** v1.0 — Owner scope decision, 2026-10-25
**Trạng thái:** APPROVED FOR IMPLEMENTATION: P0, P1, P2, P3, P4A; HOLD: P4B, UX độc lập, P5. Không cấp quyền commit/push/merge/deploy.
**Sản phẩm:** Avatar_Runtime / Native Avatar Studio / Studio Local Agent Toolkit
**Nhánh đã đối chiếu:** `exp/g5-local-cas-transactions` tại `E:\Avatar_Runtime`
**Chủ quyết định:** Owner
**Người lập:** AI assistant (tổng hợp từ mã nguồn và checkpoint hiện hữu)
**Độ tin cậy (Cn):** C2 đối với cấu trúc/hiện trạng đã đọc; C1 đối với giải pháp đề xuất và các phụ thuộc chưa kiểm chứng.

> **Quyết định Owner 2026-10-25:** chỉ P0–P4A được triển khai cục bộ trên beta. DEC-01..08 đã chốt; P4B/UX độc lập/P5 HOLD. Việc duyệt phase không đồng nghĩa PASS, không cho phép phát hành hay thao tác trên MOC3 gốc.

## 1. Mục tiêu đã thống nhất và ranh giới

**Mục tiêu chính:** hoàn thiện một **Native Studio có thể thao tác trực tiếp**, sử dụng được từ cả **giao diện người dùng** và **Local Agent Toolkit** trên cùng dữ liệu và cùng quy tắc giao dịch. Ưu tiên làm ổn định liên kết **Rig ↔ Layer → Layer Editor hoàn chỉnh → Model Editor**.

**Mục tiêu song song đã được Owner chọn:** giao diện **viewport lớn**, thanh công cụ theo ngữ cảnh; panel trái/phải/dưới **có thể thu gọn**; dễ tìm chức năng nhưng **không loại bỏ chức năng** để đơn giản hóa UI. Bộ ảnh concept đã được Owner duyệt về hướng bố cục; từng trạng thái, luồng thao tác, kích thước và tính khả dụng vẫn cần kiểm thử thực tế.

**Giới hạn bắt buộc:**

- Chỉ Local API/local model; không xây lại hệ thống quản trị Agent hay dịch vụ cloud để hoàn thành Studio.
- Bảo vệ model, texture, asset, đường dẫn và dữ liệu Rig: **không công bố lên GitHub hoặc báo cáo công khai**; mọi E2E dùng bản thử nghiệm riêng có kiểm tra quyền sở hữu dữ liệu.
- **Không viết trực tiếp lên MOC3 gốc**; không đổi tên binary ARS thành `.moc3`; không tự chế công cụ vá nhị phân Cubism.
- Giữ nguyên các bài test, đường ứng dụng và công việc chưa commit của tác nhân khác; không reset/clean hàng loạt.
- **Không commit, push, merge, triển khai Host hay mở G5 writer** khi chưa có lệnh rõ ràng của Owner. Mọi bước chờ gate vẫn là HOLD.

## 2. Hiện trạng kỹ thuật — phân biệt đã có và chưa có

| Phạm vi | Có trong workspace | Bằng chứng đã ghi ở checkpoint | Thiếu / trạng thái |
|---|---|---|---|
| Rig chọn drawable / RigBank / lịch sử | Native Rig chọn layer, chuyển layer, undo/redo, generation/revision | Native test và E2E trước đó | Chưa chứng nhận toàn bộ tương tác GUI thủ công |
| Layer hình học | Sidecar riêng: tạo/xóa/nhân bản/đổi thứ tự; vertex/triangle; import snapshot từ MOC3 | C unit, Local Agent, E2E | Giới hạn hiện tại 8 layer × 128 vertex × 256 triangle; chưa là source authoring Cubism |
| UV/vật liệu | UV từng vertex; tint RGBA, opacity; checker preview | Unit + Native local E2E | Texture atlas, material gốc, PSD và parity export chưa hoàn chỉnh |
| Rig ↔ custom layer | Weight từng vertex, parent link/stiffness, pin, kiểm tra chu trình; sidecar V3 | Native Release + C unit + physical E2E | Chưa ghép đủ RigBank của MOC3 với authored layer, skinning và liên kết đa layer |
| XPBD/render | Solver riêng cho custom layer; GPU frame acceptance + kiểm tra vertex/pin | Native E2E trước đó | Chưa kết luận collision liên layer, full render parity, multi-anchor/skin source MOC3 |
| Biên dịch | Nguồn sidecar → `model_runtime.armesh` (`ARSMESH1`), kiểm tra integrity, mở lại trong Studio | Unit + cross-process Native E2E | **Không phải MOC3**; không có writer MOC3 được xác minh |
| UI/UX chung | Sidebar trái/phải thu gọn, Inspector Geometry / UV-Material / Rig-XPBD / Build | Build Native, kiểm tra source; concept đã chốt | Dock dưới, luồng phím tắt/khả năng sử dụng, ảnh đối chiếu màn hình chưa nghiệm thu |
| GUI tự động | Script xác nhận HWND/PID, foreground, cửa sổ tại tọa độ click | Có E2E trước đó | **Host hiện tại chưa đạt GUI input gate** (`SendInput` bị từ chối); không tuyên bố PASS ở bản Host mới |
| Kiểm thử hồi quy | Checkpoint gần nhất: 310 Python PASS, 19 SKIP, 229 subtests; 17/17 Native chọn lọc | Kết quả ghi trong phiên triển khai trước | Không tương đương QA toàn sản phẩm hay review độc lập; chạy lại ở mỗi release gate |

**Lưu ý:** các file đang là **working tree chưa commit** (nhiều thay đổi có trước lộ trình này). Các con số PASS chỉ đúng với **phạm vi và thời điểm đã chạy**. Không coi chúng là chứng nhận kết quả tương lai.

## 3. Kiến trúc đích (Owner duyệt có giới hạn P0–P4A)

```
                        Shared Native Studio UI
          Project Tree | Viewport | Context Inspector | Dock
                                |
                        Local Agent Toolkit
                         (same owner state)
                                |
                   Studio Document Controller
                 Revision / CAS / Validate / Undo
                                |
           +--------------------+---------------------+
           |                                          |
   Editable Authoring Project                  Runtime References
   Layer / UV / Texture / Rig                 MOC3 original: read-only
   Material / Parameters                     Local ARS compiled model
           |                                          |
    Safe draft + backups                    Native Rig / XPBD / GPU
           |                                          |
           +---------- Compile / Test ----------------+
                                |
                      Owner Release Gate
```

- **Nguồn sự thật đối với layer được tạo trong Studio:** dự án authoring có thể sửa, có ID ổn định, layer tree, revision, validation và backup. File ARS binary chỉ là **sản phẩm biên dịch**; không là nguồn biên tập chính.
- **MOC3:** giữ nguyên là tham chiếu runtime bất biến. Nếu có `.cmo3`, quy trình Cubism Editor là **luồng riêng** để sản xuất `.moc3` thực sự, phải thiết kế và phê duyệt sau khi kiểm tra công cụ.
- **Một cơ chế thao tác:** UI và Local Agent cùng dispatcher/document revision; undo/redo áp dụng nhất quán; không cho phép UI bỏ qua fence Agent hay ngược lại.
- **Hai lớp xác nhận:** `validate → preview → commit authoring document`, tiếp theo `compile → reopen → native frame acceptance` trên bản sao; chỉ sau đó mới xét xuất bản.
- **Rig/XPBD:** mô tả liên kết theo ID layer/vertex ổn định, quản lý mất tham chiếu khi delete/reorder, hỗ trợ trạng thái dirty/rebind; tránh lấy vị trí danh sách làm danh tính vĩnh viễn.

## 4. Lộ trình theo thứ tự phụ thuộc

### P0 — Chuẩn hóa baseline và hợp đồng dữ liệu (đầu vào bắt buộc)

**Owner:** APPROVED FOR IMPLEMENTATION — beta/local, theo gate phụ thuộc.

**Mục đích:** chốt kiến trúc đã thay đổi, bảo đảm không mất dữ liệu hay khởi động sai bài toán.

- Kiểm kê API thao tác thật so với UI và test; phân loại **shipping / test-only / HOLD**, phát hiện thao tác trùng.
- Chuẩn hóa khái niệm MOC3 read-only, editable sidecar/source, ARSMESH1 binary, backup, nguồn `.cmo3` của Cubism (nếu có).
- Thiết kế ID layer/vertex không thay đổi sau reorder; contract di chuyển layer, đổi topology, collision dữ liệu, migration V1→V2→V3.
- Lập ma trận feature→UI→Local API→Native consumer→test, bao gồm xử lý session/revision/UNKNOWN_OUTCOME.

**Điều kiện PASS:** tài liệu nguồn dữ liệu, ma trận API, sơ đồ phụ thuộc và danh sách finding có chủ xử lý; **không** tác động model thật. **Gate:** Owner duyệt các quyết định DEC trong mục 7 trước khi triển khai phần làm thay đổi định dạng hoặc nguồn dữ liệu.

### P1 — Hoàn thiện Rig ↔ Layer Editor (ưu tiên chức năng số 1)

**Owner:** APPROVED FOR IMPLEMENTATION — beta/local, theo gate phụ thuộc.

- Ràng buộc Rig vào layer theo ID ổn định; parent/child, weight, anchor, liên kết layer–layer, tái ánh xạ sau thay đổi mesh; hành vi rõ ràng khi xóa vertex hoặc layer.
- Đồng bộ thao tác **chọn/move weight/pin/parent/undo/redo** giữa viewport, Inspector, Agent, và lần mở lại dự án.
- Mở rộng constraint và skinning từng bước; test hierarchy acyclic, CAS, ABA, topology edit khi Rig đang gắn, rollback không mất rig.
- Tích hợp playback/preview XPBD và render witness; chỉ ghi `APPLIED` khi solver bind đúng revision và GPU frame đã được chấp nhận.

**Điều kiện PASS:** E2E trên Studio thật với ít nhất hai layer có liên kết, di chuyển một layer không hỏng layer khác, reload/restart giữ đúng Rig, stale request bị chặn, hậu kiểm frame và undo/redo; không có bypass mô hình gốc. **Gate:** độc lập review diff và không còn lỗi blocker về dữ liệu.

### P2 — Layer Editor hoàn chỉnh (ưu tiên chức năng số 2)

**Owner:** APPROVED FOR IMPLEMENTATION — beta/local, theo gate phụ thuộc.

- Layer tree: tạo, chọn, đổi tên, nhân bản, xóa, nhóm, thứ tự hiển thị/đè lớp, isolate/visibility và thao tác nhiều layer.
- Mesh: thêm/xóa/sửa vertex/triangle, di chuyển hàng loạt, kiểm tra topology, cảnh báo khi thao tác phá liên kết Rig/UV/XPBD.
- UV: chọn điểm/tam giác, transform/map UV, biên tập atlas/texture tham chiếu bằng file local, preview và tính nhất quán sau lưu/mở lại.
- Material: tint/opacity và texture source/slot, blend/draw order nếu nguồn hỗ trợ, không coi checker preview là texture thật.
- Undo/redo, draft/validate/commit, backup tự phục hồi, di trú dữ liệu, cảnh báo giới hạn mesh, trạng thái unsaved rõ ràng.

**Điều kiện PASS:** mọi chức năng có cả thao tác UI hoặc UI inspector thực sự, Agent API tương ứng khi phù hợp, save/restart/undo, stress với nhiều layer và model private; không có geometry bị mất hoặc UV/triangle out-of-bounds. **Gate:** Owner nghiệm thu thao tác và phạm vi giới hạn hiệu năng.

### P3 — XPBD liên layer + nghiệm thu render

**Owner:** APPROVED FOR IMPLEMENTATION — beta/local, theo gate phụ thuộc.

- Xây đồ thị constraint/collision giữa layer, Neo nhiều vùng, stiffness/damping theo layer, bảo toàn layer order và quyền giữ mesh nguồn.
- Kiểm thử định lượng displacement, tốc độ/jitter ở 60 FPS mục tiêu, collision xuyên lớp, frame-fence, GPU output acceptance và Debug/Release.
- Thiết kế công cụ QA thực tế: script tự khởi chạy Studio riêng, theo PID/HWND, test trong vùng cửa sổ thử nghiệm; không chạm cửa sổ chạy Host hay ứng dụng cá nhân.

**Điều kiện PASS:** định lượng được frame, collision, anchor drift, không replay frame sai revision, xuất log/ảnh QA nội bộ **không tiết lộ model**. **Gate:** nếu Desktop Control không có quyền hoặc không xác minh foreground, đánh dấu **BLOCKED**, không giả mạo GUI PASS.

### P4 — Model Editor và quy trình biên dịch nguồn (ưu tiên chức năng số 3)

**Owner:** P4A APPROVED FOR IMPLEMENTATION; P4B HOLD theo DEC-02=B.

**Nhánh A — Native Avatar Studio (có thể chủ động phát triển):**

- Authoring project chứa layer tree, texture, UV, vật liệu, Rig/XPBD và phiên bản nguồn.
- Biên dịch ra ARSMESH1 / runtime package nội bộ, kiểm tra hash/format, đóng gói resource, mở lại bằng Native Studio, rollback từ backup.
- Thêm thao tác model-level: source material, parameter/pose, cấu trúc scene, validate trước compile, báo cáo tương thích.

**Nhánh B — Cubism `.cmo3` → `.moc3` thật (phụ thuộc công cụ và quyền):** **[P4B HOLD; KHÔNG TRIỂN KHAI]**

- Xác nhận Owner có **file `.cmo3` có thể chỉnh sửa**, Cubism Editor được phép sử dụng và đường export chính thức.
- Khảo sát giao diện/tích hợp Editor cho **sửa nguồn → export MOC3 + model3.json + texture → mở lại Runtime Studio**; thí nghiệm chỉ trên bản sao private.
- Nếu không có công cụ viết `.cmo3` được hỗ trợ, chỉ cung cấp chức năng chuyển giao/đối chiếu nguồn và hướng dẫn export **thủ công trong Cubism Editor**; không khẳng định Studio đã biên dịch `.moc3`.

**Điều kiện PASS A:** roundtrip project→binary→Studio, rollback, asset manifest, model parity đo được, không sửa original. **Điều kiện PASS B:** MOC3 do Cubism Editor xuất hợp lệ, khớp nguồn `.cmo3` và texture/model3.json; có bằng chứng reopen/visual/SDK acceptance. Hai nhánh được báo cáo riêng, **không thay thế chứng nhận cho nhau**.

### UX — Luồng giao diện chung (triển khai song song P1→P4)

**Owner:** UX độc lập HOLD; DEC-06=A duyệt chuẩn thiết kế viewport-first, không cấp quyền cho cả phase UX. UI tối thiểu phục vụ P1–P4A nằm trong phase tương ứng.

- Khung chung: menu/top toolbar; project/layer tree bên trái; viewport ưu tiên trung tâm; Inspector context bên phải; dock dưới cho history/timeline/validation; tất cả panel thu gọn/khôi phục được.
- Workspace tabs dùng chung: Layer, Rig, UV/Material, XPBD/Physics, Model/Compile. Các thao tác quan trọng luôn truy cập được kể cả khi panel thu gọn, có trạng thái nguồn **read-only vs editable**.
- Tài liệu layout: ảnh Owner đã duyệt là chuẩn thẩm mỹ; `docs/ui/AVATAR_STUDIO_SHARED_LAYOUT.svg` là sơ đồ bổ trợ, **không tự nhận là các ảnh mẫu gốc**. Từng vùng cần mockup, state diagram (empty/loading/dirty/validation/conflict/blocked) và shortcut map.
- Tối ưu thao tác click/drag, resize, dock, phím tắt; hiển thị progress/validation của các lệnh Local Agent. Không tạo tool UI chỉ làm giả thao tác backend.

**Điều kiện PASS:** ma trận chức năng không có đường UI chết, thử ít nhất 1280×720 và 1920×1080, screenshot-diff/manual review khi có quyền, PID/foreground gate và Agent/UI parity. **Không PASS bằng ảnh concept hoặc unit test đơn thuần.**

### P5 — Ổn định, review và release gate (chỉ sau P1–P4 phù hợp phạm vi Owner duyệt)

**Owner:** HOLD, không cho triển khai phase release; các test/review cần thiết trong P0–P4A vẫn bắt buộc.

- Làm sạch cảnh báo mới, kiểm thử crash-recovery, backup, private-model publication, Windows Release/Debug, độc lập review, phản hồi finding.
- Không làm hỏng Studio cũ/Native Viewer, SDK ABI và workspace của tác nhân khác.
- Đưa bằng chứng PASS/HOLD riêng từng chức năng; CI không chạy thì nêu ngoại lệ kiểm thử cụ thể, không suy diễn xanh toàn bộ.
- Chỉ khi Owner ra lệnh mới commit/merge/push/release; cập nhật hướng dẫn chạy thử Host riêng.

**Điều kiện PASS:** có release checklist được Owner ký duyệt, không còn finding blocker, E2E đúng ứng dụng/cửa sổ, dữ liệu private không lộ, rollback được chứng minh.

## 5. Quan hệ phụ thuộc và cách phát triển song song

```text
P0 (baseline + phê duyệt kiến trúc)
  ├─ P1 Rig ↔ Layer ─→ P2 Layer Authoring ─→ P3 Multi-Layer XPBD
  │                                              └─→ P4A Native Model Editor
  ├─ P4B Cubism source/export (HOLD theo DEC-02=B)
  └─ UX độc lập (HOLD); UI thiết yếu thuộc P1–P4A
                                               ↓
                                    P5 Review / Release Gate
```

- Mỗi phase có checkpoint commit **đề xuất** nhưng chỉ thực hiện nếu Owner cho phép; trước khi được duyệt, làm việc trên working tree và báo cáo rõ tác động.
- Không chuyển phase vì đã hết checklist giấy; yêu cầu **Native E2E có chứng cứ**, không nâng trạng thái HOLD lên PASS nếu chỉ có mockup hoặc test harness.
- Khi GUI input bị chặn, có thể tiếp tục E2E qua Local API/Native riêng trong phạm vi được duyệt, nhưng **GUI nghiệm thu vẫn HOLD**.

## 6. Ma trận kiểm chứng tối thiểu

| Gate | Phải chứng minh | Bằng chứng yêu cầu |
|---|---|---|
| DATA | Không thay model gốc, migration V1/V2/V3, corruption rejection, backup/recovery | checksum nguồn, diff/fixture, test restart |
| ID/CAS | ID ổn định, stale generation/revision, rollback/ABA, đổi layer/topology | unit + Native E2E, outcome receipts |
| RIG | Weight/parent/cross layer đúng, xóa tham chiếu không làm sai rig | two-layer physical E2E, snapshot diff |
| UV/MAT | UV biến đổi thật, đúng atlas/texture, material preview parity | source/reopen/render frame |
| XPBD/GPU | Vertex motion, collision, anchor drift, đúng revision, GPU accepted frame | quantitative E2E + frame acceptance |
| UI/UX | Đúng cửa sổ/PID; thao tác được, panel không che/mất tool | GUI automated + owner visual review |
| MODEL-A | ARS binary đúng source, compile/validate/reopen/rollback | independent Native consumer |
| MODEL-B | `.moc3` hợp lệ do pipeline Cubism được hỗ trợ tạo ra | Cubism export evidence + SDK load |
| PRIVACY | Không leak model/texture/IDs/raw geometry qua logs/repo | publication guard + repository scan |
| RELEASE | Build/CI/review/recovery, không phá ABI/Studio cũ | checklist + Owner approval |

## 7. Các quyết định Owner — ĐÃ CHỐT (xem bảng quyết định bổ sung)

| Mã | Câu hỏi | Đề xuất A (tham chiếu lịch sử) | Nếu không duyệt |
|---|---|---|---|
| **DEC-01** | Chọn nguồn chuẩn của model mới? | **A:** dự án authoring Native Studio là nguồn chính; ARS binary chỉ là output, MOC3 chỉ-read | **B:** chỉ tích hợp Cubism Editor source `.cmo3` |
| **DEC-02** | Mục tiêu `.moc3` thật và quyền sử dụng Cubism? | **A:** tách nhánh nghiên cứu Cubism; chỉ cho phép export chính thức trên bản sao sau khi xác nhận `.cmo3` + Editor | **B:** hoãn xuất MOC3, tập trung ARS Native |
| **DEC-03** | Mức liên kết Rig ưu tiên? | **A:** ổn định Rig trong/giữa custom layer + skinning/multi-anchor trước, sau mới mapping RigBank MOC3 | **B:** ưu tiên mapping với RigBank gốc trước |
| **DEC-04** | Phạm vi vật liệu và texture? | **A:** có file texture/atlas local thật, UV parity, alpha/blend/draw order; checker chỉ là debug | **B:** tạm chỉ RGBA/UV không texture source |
| **DEC-05** | Giới hạn layer/vertex và history hiện tại? | **A:** coi 8 × 128 × 256 và 6 undo là giới hạn beta cần mở rộng sau benchmark | **B:** cố định các giới hạn này cho MVP |
| **DEC-06** | Chốt tiêu chuẩn UI/UX? | **A:** viewport-first đã chọn, panel 3 cạnh thu gọn, desktop layout dùng chung, giao diện chức năng theo ngữ cảnh; nghiệm thu trực tiếp từng vùng | **B:** chỉ hoàn thiện Inspector hiện có trước |
| **DEC-07** | Khi Desktop Control/GUI input chưa qua gate? | **A:** tiếp tục Native/Agent test; GUI HOLD cho đến khi có quyền hợp lệ, không bypass | **B:** dừng cả luồng triển khai đến khi GUI hoạt động |
| **DEC-08** | Phê duyệt từng phase và quyền tích hợp Git? | **A:** phê duyệt thiết kế từng phase riêng; không commit/push/merge cho đến lệnh rõ | **B:** Owner chỉ định một phạm vi khác |

### Quyết định thực tế của Owner — APPROVED (2026-10-25)

Các phương án A/B ở bảng trên là bản gốc để đối chiếu; **bảng dưới đây là lựa chọn có hiệu lực**.

| Mã | Owner chọn | Hành vi đã chốt | Trạng thái |
|---|---|---|---|
| DEC-01 | **A** | Authoring Native Studio là nguồn chuẩn; ARS binary chỉ output, MOC3 gốc read-only. | APPROVED |
| DEC-02 | **B** | Hoãn xuất MOC3/Cubism, tập trung Native ARS; P4B HOLD. | APPROVED |
| DEC-03 | **A** | Ưu tiên Rig trong/giữa custom layer, skinning, multi-anchor; RigBank MOC3 mapping sau. | APPROVED |
| DEC-04 | **A** | Texture/atlas local thực, UV parity, alpha, blend, draw order; checker chỉ debug. | APPROVED |
| DEC-05 | **A** | 8 layer × 128 vertex × 256 triangle, 6 undo chỉ là giới hạn beta; benchmark rồi mở rộng. | APPROVED |
| DEC-06 | **A** | Viewport-first, 3 vùng panel thu gọn, công cụ ngữ cảnh, nghiệm thu UI từng vùng; UX độc lập vẫn HOLD. | APPROVED |
| DEC-07 | **A** | Tiếp tục Native/Agent E2E nếu Host chặn GUI; GUI HOLD, không bypass hay click ứng dụng cá nhân. | APPROVED |
| DEC-08 | **B** | Owner chỉ định phạm vi riêng: P0–P4A APPROVED; P4B/UX độc lập/P5 HOLD. Không cấp quyền Git/release. | APPROVED |

**Cho phép thực hiện:** kiểm toán, thiết kế, sửa mã và kiểm thử cục bộ trong P0→P1→P2→P3→P4A theo gate; UI tối thiểu phải hoạt động để nghiệm thu tính năng đã duyệt.

**Không cho phép:** triển khai P4B, UX độc lập, P5; stage/commit/push/merge, deploy, G5 writer, sửa nhị phân MOC3 gốc, xuất bản dữ liệu model hoặc dọn/reset working tree của tác nhân khác.

**Không gộp các câu trả lời DEC thành quyền triển khai.** Owner có thể duyệt một số quyết định và để các quyết định còn lại HOLD. Quyền sửa mã, mở Cubism Editor trên model riêng, commit và phát hành được xin **riêng theo phase**.

## 8. Rủi ro và điểm dừng bắt buộc

| Rủi ro | Phòng ngừa / điểm dừng |
|---|---|
| Lầm ARS binary là `.moc3` | Tách format / nhãn / test consumers; B chỉ chấp nhận Cubism Editor xuất thật |
| Sai nguồn texture hoặc mất quyền đối với `.cmo3` | Kiểm kê trước; không tự mở rộng ra server/public; dừng thử nghiệm với model thật khi không rõ quyền |
| Hỏng Rig sau reorder/delete vertex | Stable IDs, constraints validation, CAS, preview + undo, E2E nhiều layer |
| Backup chưa crash-durable | Bổ sung durable write/fsync/atomic rename + recovery test trước mở writer quan trọng |
| Nhầm frame CPU prepare với GPU accepted | Native render fence/GL status/snapshot oracle, không ghi APPLIED sớm |
| GUI test đi nhầm cửa sổ | PID/HWND/foreground xác minh ngay trước input; STOP khi không đúng |
| Host từ chối SendInput | HOLD GUI gate; không dùng cách vượt quyền hoặc thao tác ứng dụng cá nhân |
| Dữ liệu model rò rỉ | Source/test asset nội bộ; log chỉ metadata không nhạy cảm; scan trước commit |
| Phình G5 hoặc phá ABI | Chỉ mở rộng contract cần cho Studio; ưu tiên module sản phẩm và test hồi quy |
| Chồng lấn working tree nhiều tác nhân | Không `reset`/`clean`/stage tất cả; diff scope rõ trước mọi hành động Git |

## 9. Biên bản Owner phê duyệt và kích hoạt

**Ngày 2026-10-25:** APPROVED FOR IMPLEMENTATION: P0, P1, P2, P3, P4A; DEC-01=A, DEC-02=B, DEC-03=A, DEC-04=A, DEC-05=A, DEC-06=A, DEC-07=A, DEC-08=B.

**Tiếp tục HOLD:** P4B (Cubism/MOC3 export), UX độc lập, P5 (release). DEC-06=A chốt hướng UI nhưng không mở UX như một phase riêng. DEC-08=B là phạm vi Owner chỉ định, không phải quyền Git.

**Bước thực hiện tiếp theo được phép:** P0 kiểm kê source ownership, API/feature matrix, stable IDs, CAS/ABA, migration và finding. Chỉ sau gate P0 mới chuyển P1, rồi tuần tự P2/P3/P4A với bằng chứng Native E2E; mọi gate không có bằng chứng vẫn ghi HOLD.

**Giữ nguyên:** không commit, push, merge, deploy, reset/clean workspace, công khai model, sửa MOC3 gốc hoặc bật G5 writer nếu Owner chưa ra lệnh riêng.

**Phê duyệt lộ trình không có nghĩa các phase đã PASS hoặc sản phẩm đã ready.**

## 10. Nguồn đối chiếu

**Repository (đã đọc):**

- `docs/ACTIVE_STUDIO_GOAL.md`
- `docs/in-progress/studio-beta-20261023/LAYER_RIG_NATIVE_MODEL_RUNTIME_20261025.md`
- `docs/in-progress/studio-beta-20261023/SHARED_UI_UV_MATERIAL_XPBD_20261025.md`
- `docs/in-progress/studio-beta-20261023/LAYER_EDITOR_LOCAL_GEOMETRY_20261025.md`
- `tools/avatar-studio/studio_geometry_document.h`
- `tools/avatar-studio/studio_geometry_runtime.c`
- `tools/avatar-studio/studio_geometry_xpbd.cpp`
- `tools/avatar-studio/studio_rig_dispatch.c` / `studio_rig_history.c`
- `scripts/test_studio_geometry_uv_physics_live.py` / `test_studio_geometry_gui_win32.py`
- `docs/ui/AVATAR_STUDIO_SHARED_LAYOUT.svg`

**Nguồn chính thức kiểm tra định dạng, cập nhật 2026-10-13:**

- Live2D Cubism Editor, *File Types and Extensions*: https://docs.live2d.com/en/cubism-editor-manual/file-type-and-extension/
- Live2D Cubism Editor, *Data for Embedded Use*: https://docs.live2d.com/en/cubism-editor-manual/export-moc3-motion3-files/

**Giới hạn đánh giá:** không chạy lại toàn bộ bộ test trong lượt **chỉ lập lộ trình** này; bằng chứng PASS thuộc các checkpoint trước. Đề xuất không hàm ý đã kiểm chứng workflow Cubism `.cmo3` trên máy người dùng.