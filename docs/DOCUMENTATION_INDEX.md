# Mục lục và phân loại tài liệu

Rà soát ngày **21-10-2026, Asia/Bangkok**, nhánh `alpha`. Inventory hiện tại: **160 tệp**, gồm `README.md` và toàn bộ `docs/`. Mốc phân loại ban đầu có 125 tệp; PUB-02-A/B + PUB-04-A/B + alpha audit thêm 35 tài liệu. Xem checkpoint mới nhất ở cuối trang và catalog cho danh sách đầy đủ.

Việc phân loại đánh giá vai trò hiện tại của tài liệu. “Đã hoàn thành” chỉ áp dụng cho phạm vi component/private/test ghi ở từng dòng, không tuyên bố milestone, G00–G11 hay SDK đã hoàn tất. “Tham khảo” gồm cả contract đang áp dụng và chứng cứ lịch sử; “lỗi thời” nghĩa là không còn dùng làm kế hoạch/trạng thái hiện hành. Tài liệu đã được chuyển sang bốn thư mục theo phân loại; raw log và JSON chứng cứ giữ nguyên bytes/pins. Markdown chỉ đổi liên kết tương đối. [Bản đồ đường dẫn](DOCUMENTATION_PATHS.json) lưu legacy → canonical và hash trước/sau. Các đường dẫn trong snapshot Git/JSON lịch sử vẫn mô tả source gốc.

## Nên đọc từ đâu

| Nhu cầu | Tài liệu |
| --- | --- |
| Trạng thái gate/giới hạn hiện tại của optional MOC3 | [M2_MOC3_F4_RELEASE_LEDGER.md](in-progress/M2_MOC3_F4_RELEASE_LEDGER.md) |
| Việc triển khai tiếp theo | [M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md](in-progress/M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md) |
| Phạm vi static admission vừa nghiệm thu | [M2_MOC3_F4_PACKAGE_ADMISSION_DESIGN.md](completed/M2_MOC3_F4_PACKAGE_ADMISSION_DESIGN.md) và [M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md](completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md) |
| Target framework / API quan sát | [M2_MOC3_FRAMEWORK_TARGET_DESIGN.md](in-progress/M2_MOC3_FRAMEWORK_TARGET_DESIGN.md) và [M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md](in-progress/M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md) |
| Gate/thiết kế tổng thể | [ROADMAP_AND_ADRS_V0.1.md](in-progress/ROADMAP_AND_ADRS_V0.1.md), [ACCEPTANCE_GATES_V0.1.md](reference/ACCEPTANCE_GATES_V0.1.md), [V0_1_CANONICAL_CONTRACT_RC1.md](reference/V0_1_CANONICAL_CONTRACT_RC1.md) |
| D06/D08 physical HOLD | [M2_D06_D08_LOSS_VERIFICATION_DESIGN.md](in-progress/M2_D06_D08_LOSS_VERIFICATION_DESIGN.md) và [M1_D_GPU_CASE_EXECUTION.json](in-progress/M1_D_GPU_CASE_EXECUTION.json) |
| Inventory máy đọc được | [DOCUMENTATION_CATALOG.json](DOCUMENTATION_CATALOG.json) |

## Tổng hợp bốn nhóm tại mốc phân loại ban đầu

| Nhóm | Có sẵn | Mục lục mới | Tổng |
| --- | ---: | ---: | ---: |
| Đã hoàn thành trong phạm vi đã nêu | 27 | 0 | 27 |
| Tham khảo / contract / chứng cứ lịch sử | 76 | 3 | 79 |
| Lỗi thời với vai trò kế hoạch hiện hành | 4 | 0 | 4 |
| Đang phát triển / triển khai / chờ kiểm chứng | 15 | 0 | 15 |

## Các điểm lệch trạng thái cần biết

| Mục | Đối chiếu và cách đọc |
| --- | --- |
| Governance đã có trong fetched main | `origin/main` local được fetch ở bước commit/push trước có SHA `8723ef1`; file policy index tồn tại và approved proposal head `57fbcc7` là ancestor. Các header `PENDING_MAIN`/`CANONICAL_IN_MAIN=NO` là bối cảnh cũ. Đây là kiểm tra Git local, không phải audit GitHub live/protection. Các quy tắc authority/merge vẫn giữ nguyên. |
| M1-A ghi chưa triển khai | [M1_A_SCOPE_V0.1.md](completed/M1_A_SCOPE_V0.1.md) còn `IMPLEMENTATION_NOT_STARTED`; [M1_A_BUILD_EVIDENCE.md](completed/audits/M1_A_BUILD_EVIDENCE.md) và CMake/source foundation cho thấy phạm vi foundation đã triển khai. Public SDK chưa hoàn thành. |
| Harness có phần mở đầu cũ | [M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md](in-progress/M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md) còn HOLD continuous control ở đầu; phần F4 follow-on, source `CORE-LIVE` và [M2_MOC3_F4_COMPLETION_PROGRESS.md](reference/audits/M2_MOC3_F4_COMPLETION_PROGRESS.md) đã ghi controller trusted-local PASS. Public persistent evaluator vẫn chưa xong. |
| “Chưa commit”, PR draft, no-push trong báo cáo cũ | Những câu này mô tả thời điểm chạy. Source F3/F4/static đã commit/push trong `6fdec9b`; không đổi hash/source snapshot cũ để giả thành chứng cứ của HEAD mới. |
| Kế hoạch MOC3 cũ | [M2_MOC3_NEXT_IMPLEMENTATION.md](obsolete/M2_MOC3_NEXT_IMPLEMENTATION.md) còn nói chưa có host snapshot/pixels/physics; dùng F1–F4/PUB-* để chọn việc tiếp theo. Fidelity, public trust và device-loss chưa được đóng chỉ vì plan cũ bị thay. |
| Security R2/R3/R4 không bỏ toàn bộ | R5 chỉ supersede những phần nêu rõ; provenance/registry, rotation, terminal expiry và clock poison được kế thừa theo thứ tự amendment. Vì vậy xếp tham khảo có phần superseded, không đánh dấu toàn file vô hiệu. |
| F4 local physics khác public static physics | F3/private F4 có physics execution; static PUB-01/PUB-03 chỉ **VALIDATED_NOT_SIMULATED**, atlas tối đa 128 MiB decoded. Whole F4, SDK, integrated ASAN/fuzz, performance/platform và D06/D08 physical tiếp tục HOLD/REVISE. |
| DEVELOPMENT và dependency checkpoint | Số `77` Python / `five` native trong [DEVELOPMENT.md](reference/DEVELOPMENT.md) chỉ mô tả PREP cũ; `c1_ready/c2_ready` trong [M1_C_DEPENDENCY_LOCK.json](reference/M1_C_DEPENDENCY_LOCK.json) là checkpoint. Không dùng chúng làm tổng test hay trạng thái sản phẩm hôm nay. |

## Kiểm tra và bảo toàn chứng cứ

- Đọc inventory/headers/status, supersession và kết luận scope; đối chiếu source/CMake/API hiện có cùng báo cáo tương ứng. Không xếp nhóm chỉ theo tên file hay từ PASS/HOLD.
- 22 cặp hash raw-log được khai báo trong các bảng báo cáo đều khớp. Không suy rộng thành kiểm chứng mọi source/binary pin hay chạy lại toàn bộ runtime.
- Các link Markdown tương đối được kiểm tra ở checkout; không có link đích bị thiếu. Link web, anchor heading và trạng thái PR/CI live không nằm trong kiểm tra này.
- 8 raw log dùng UTF-16 được giữ nguyên bytes/encoding. Báo cáo/hashes lịch sử giữ tại đường dẫn cũ. Tệp source/binary manifest là chứng cứ của lần chạy được ghi, không phải cam kết hash luôn khớp HEAD.
- Cấu trúc hiện tại: `completed/`, `reference/`, `obsolete/`, `in-progress/`; các bộ đọc file hiện tại dùng đường dẫn mới. Không thay gate verdict, nội dung pin/JSON/raw evidence hoặc private model/ảnh. Lịch sử Git vẫn truy xuất bằng đường dẫn cũ tại revision cũ.

## Danh mục từng tài liệu

### Đã hoàn thành trong phạm vi đã nêu

| Tài liệu | Phạm vi / lý do / phần cần giữ |
| --- | --- |
| [M1_A_SCOPE_V0.1.md](completed/M1_A_SCOPE_V0.1.md) | M1-A build/ABI foundation đã có triển khai và chứng cứ; không phải runtime SDK đầy đủ. → [M1_A_BUILD_EVIDENCE.md](completed/audits/M1_A_BUILD_EVIDENCE.md) |
| [M2_C_GLB_RENDER_DESIGN.md](completed/M2_C_GLB_RENDER_DESIGN.md) | Private C0–C2 GLB projection/render đã có chứng cứ; G04/public integration còn HOLD. → [M2_C_IMPLEMENTATION_EVIDENCE.md](completed/audits/M2_C_IMPLEMENTATION_EVIDENCE.md) |
| [M2_MOC3_F1_ADMISSION_DESIGN.md](completed/M2_MOC3_F1_ADMISSION_DESIGN.md) | F1 trusted-local CPU admission/pose đã PASS trong phạm vi đã chốt. → [M2_MOC3_F1_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F1_LOCAL_ACCEPTANCE.md) |
| [M2_MOC3_F2_PIXEL_DESIGN.md](completed/M2_MOC3_F2_PIXEL_DESIGN.md) | F2 bounded host pixels đã PASS; giữ làm contract cho renderer private. → [M2_MOC3_F2_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F2_LOCAL_ACCEPTANCE.md) |
| [M2_MOC3_F3_LINKED_PHYSICS_DESIGN.md](completed/M2_MOC3_F3_LINKED_PHYSICS_DESIGN.md) | F3 linked physics private đã PASS; chưa chứng minh tương đương tác giả/Cubism. → [M2_MOC3_F3_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F3_LOCAL_ACCEPTANCE.md) |
| [M2_MOC3_F4_CONTINUITY_DESIGN.md](completed/M2_MOC3_F4_CONTINUITY_DESIGN.md) | F4-B controller/continuity trusted-local đã có chứng cứ PASS trong checkpoint lịch sử. → [M2_MOC3_F4_COMPLETION_PROGRESS.md](reference/audits/M2_MOC3_F4_COMPLETION_PROGRESS.md) |
| [M2_MOC3_F4_INTEGRATION_DESIGN.md](completed/M2_MOC3_F4_INTEGRATION_DESIGN.md) | F4-A core → physics → host đã PASS; whole F4/public SDK còn thiếu. → [M2_MOC3_F4_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F4_LOCAL_ACCEPTANCE.md) |
| [M2_MOC3_F4_PACKAGE_ADMISSION_DESIGN.md](completed/M2_MOC3_F4_PACKAGE_ADMISSION_DESIGN.md) | Static PUB-01/PUB-03 đã PASS; physics chỉ validated, không simulated. → [M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md](completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md) |
| [M1_A_BUILD_EVIDENCE.md](completed/audits/M1_A_BUILD_EVIDENCE.md) | Báo cáo build/ABI foundation, giới hạn rõ chưa phải SDK hoạt động. |
| [M1_B1_BUILD_EVIDENCE.md](completed/audits/M1_B1_BUILD_EVIDENCE.md) | B1 registry/lifecycle CPU components; không đóng toàn bộ 87 case. |
| [M1_B2_BUILD_EVIDENCE.md](completed/audits/M1_B2_BUILD_EVIDENCE.md) | B2 source authority CPU components; không suy ra public source activation. |
| [M1_B3_BUILD_EVIDENCE.md](completed/audits/M1_B3_BUILD_EVIDENCE.md) | B3 provenance/publication CPU và hai P2 đã xử lý trong phạm vi báo cáo. |
| [M1_C1_BUILD_EVIDENCE.md](completed/audits/M1_C1_BUILD_EVIDENCE.md) | C1 private CPU ingestion/schema helper; full G02/G08 vẫn riêng. |
| [M1_C2_BUILD_EVIDENCE.md](completed/audits/M1_C2_BUILD_EVIDENCE.md) | C2 private GLB/dual CPU implementation; mô tả fuzz/CI không thay chứng cứ campaign đã chạy. |
| [M2_A_BUILD_EVIDENCE.md](completed/audits/M2_A_BUILD_EVIDENCE.md) | Private 2D CPU preparation; không phải render/public gate PASS. |
| [M2_B_BUILD_EVIDENCE.md](completed/audits/M2_B_BUILD_EVIDENCE.md) | Private 2D offscreen pixel proof; G03 sản phẩm chưa được đóng. |
| [M2_C_IMPLEMENTATION_EVIDENCE.md](completed/audits/M2_C_IMPLEMENTATION_EVIDENCE.md) | Private C0–C2 3D proof; các phần qualification còn thiếu được giữ riêng. |
| [M2_D_IMPLEMENTATION_EVIDENCE.md](completed/audits/M2_D_IMPLEMENTATION_EVIDENCE.md) | Private dual-mode transaction proof; public G00/G01/G02/G05 và loss thật còn HOLD. |
| [M2_MOC3_F1_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F1_LOCAL_ACCEPTANCE.md) | F1 trusted-local PASS trên source snapshot lịch sử. |
| [M2_MOC3_F2_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F2_LOCAL_ACCEPTANCE.md) | F2 bounded trusted-local pixel PASS trên source snapshot lịch sử. |
| [M2_MOC3_F2_RECHECK_AND_OWNER_DIAGNOSTICS.md](completed/audits/M2_MOC3_F2_RECHECK_AND_OWNER_DIAGNOSTICS.md) | F2 recheck/owner diagnostic tests đã chạy, snapshot lịch sử. |
| [M2_MOC3_F3_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F3_LOCAL_ACCEPTANCE.md) | F3 bounded linked physics private PASS trên source snapshot lịch sử. |
| [M2_MOC3_F4_LOCAL_ACCEPTANCE.md](completed/audits/M2_MOC3_F4_LOCAL_ACCEPTANCE.md) | F4-A private core/host integration PASS; không phải whole F4. |
| [M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md](completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md) | Static package admission PASS, WARP/hardware/API/UI; giữ giới hạn atlas và physics. |
| [M2_MOC3_F4_PACKAGE_ADMISSION_GOAL_ASSESSMENT.json](completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_GOAL_ASSESSMENT.json) | Goal PASS cho static admission; parent product goal vẫn REVISE. → [M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md](completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md) |
| [M2_MOC3_F4_PACKAGE_ADMISSION_REVIEW.md](completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_REVIEW.md) | Independent review PASS cho static PUB-01/PUB-03, không phải phê duyệt activation. |
| [M2_MOC3_T0_HARNESS_EVIDENCE.md](completed/audits/M2_MOC3_T0_HARNESS_EVIDENCE.md) | Synthetic T0 harness proof; không thay chứng cứ physics/render model thật. |

### Tham khảo / contract / chứng cứ lịch sử

| Tài liệu | Phạm vi / lý do / phần cần giữ |
| --- | --- |
| [README.md](../README.md) | Điểm vào hiện hành, đã cập nhật trạng thái private components và link tới catalog/ledger. |
| [ACCEPTANCE_GATES_V0.1.md](reference/ACCEPTANCE_GATES_V0.1.md) | Định nghĩa G00–G11 và mức chứng cứ; đây là criteria, không phải bảng PASS hiện tại. |
| [ADR_0001_D3D11_G00_DESIGN_RC1.md](reference/ADR_0001_D3D11_G00_DESIGN_RC1.md) | Host-owned D3D11/retirement design reference; trạng thái test đọc ở ledger/evidence mới hơn. |
| [ADR_P0_SECURITY_R2.md](reference/ADR_P0_SECURITY_R2.md) | ADR security R2–R5 và quan hệ amendment; không tự cấp phê duyệt hay đóng gate. |
| [CUBISM_STATIC_PREVIEW_IMPORT.md](reference/CUBISM_STATIC_PREVIEW_IMPORT.md) | Luồng portrait/static preview riêng; vẫn có giá trị tham khảo, không chứng minh MOC drawable/physics. |
| [DEVELOPMENT.md](reference/DEVELOPMENT.md) | Hướng dẫn PREP còn hữu ích; số test và mô tả chỉ có stub là snapshot cũ, không phải toàn checkout. |
| [DOCUMENTATION_CATALOG.json](DOCUMENTATION_CATALOG.json) | Inventory máy đọc được cùng phạm vi với mục lục; không thay ledger nghiệm thu. |
| [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) | Mục lục/phân loại và review notes được tạo trong lần rà soát này. |
| [G00_SECURITY_87_CASE_INVENTORY.json](reference/G00_SECURITY_87_CASE_INVENTORY.json) | Inventory 87 design cases; không nhầm với 87 whole-case đã chạy PASS. |
| [GOVERNANCE_ARS_SOLO_OWNER_001.md](reference/GOVERNANCE_ARS_SOLO_OWNER_001.md) | Ngoại lệ solo-owner và giới hạn authority; giữ nguyên yêu cầu source/evidence/owner decisions. |
| [M0_CONTRACT_PARENT_PINS.json](reference/M0_CONTRACT_PARENT_PINS.json) | Custody/pins lịch sử cho M0; không sửa hash để khớp code mới. |
| [M0_V0_1_FREEZE_READINESS.md](reference/M0_V0_1_FREEZE_READINESS.md) | Snapshot freeze-readiness M0; không dùng trạng thái PR/next-action lịch sử làm trạng thái live. |
| [M1_C_DEPENDENCY_LOCK.json](reference/M1_C_DEPENDENCY_LOCK.json) | Dependency/resource lock còn dùng; c1_ready/c2_ready và planned accounting là checkpoint lịch sử. |
| [M2_B_PIXEL_CONTRACT.md](reference/M2_B_PIXEL_CONTRACT.md) | UV/alpha/color/lifetime reference còn áp dụng cho private 2D; có evidence riêng đã thực thi. |
| [M2_MOC3_INDEPENDENT_RUNTIME_PLAN.md](reference/M2_MOC3_INDEPENDENT_RUNTIME_PLAN.md) | Rationale chọn independent evaluator và I0–I3 lịch sử; roadmap triển khai hiện tại là F*/PUB-*. |
| [P0_HANDLE_AND_SOURCE_AUTHORITY_CONTRACT_V0.1.md](reference/P0_HANDLE_AND_SOURCE_AUTHORITY_CONTRACT_V0.1.md) | Security contract nền, đọc cùng amendments R2–R5 và canonical RC1. |
| [P0_OWNER_REMEDIATION_V0.2.md](reference/P0_OWNER_REMEDIATION_V0.2.md) | Một phần bị R3–R5 thay thế; registry/provenance/ABI phần được kế thừa vẫn phải giữ. |
| [P0_OWNER_REMEDIATION_V0.3.md](reference/P0_OWNER_REMEDIATION_V0.3.md) | Rotation reservations/explicit revoke còn được kế thừa; clock/close phần xung đột theo R4/R5. |
| [P0_OWNER_REMEDIATION_V0.4.md](reference/P0_OWNER_REMEDIATION_V0.4.md) | Terminal expiry/clock poison còn được kế thừa; fairness/tombstone/overflow theo R5. |
| [P0_OWNER_REMEDIATION_V0.5.md](reference/P0_OWNER_REMEDIATION_V0.5.md) | Amendment security R5 còn được viện dẫn; không nhầm contract-model với production qualification. |
| [P0_SECURITY_REGRESSION_GATES_V0.1.md](reference/P0_SECURITY_REGRESSION_GATES_V0.1.md) | Các oracle/negative gates; kết quả thực thi phải đọc case ledger và evidence của đúng source. |
| [SPECIFICATION_V0.1.md](reference/SPECIFICATION_V0.1.md) | Mục tiêu/constraints SDK và các amendment; draft specification không phải khả năng đang phát hành. |
| [V0_1_CANONICAL_CONTRACT_RC1.md](reference/V0_1_CANONICAL_CONTRACT_RC1.md) | Canonical freeze-index candidate hiện hành thay RC0; không có nghĩa mọi gate đã frozen/PASS. |
| [ARS_G00_SOLO_OWNER_TECHNICAL_AUDIT_20261014.md](reference/audits/ARS_G00_SOLO_OWNER_TECHNICAL_AUDIT_20261014.md) | Technical-method audit lịch sử; không phải chứng nhận actor-distinct/current whole G00. |
| [HANDOFF_20261015.md](reference/audits/HANDOFF_20261015.md) | Snapshot PR/source ngày 15-10; không dùng số PR mở/merge state làm trạng thái hiện tại. |
| [M0_INTEGRATION_20261015.md](reference/audits/M0_INTEGRATION_20261015.md) | Custody/integration proposal tại thời điểm M0, giữ làm trace lịch sử. |
| [M1_D_BUILD_EVIDENCE.md](reference/audits/M1_D_BUILD_EVIDENCE.md) | Private GPU spike partial; source và raw logs lịch sử, không phải M1-D đầy đủ. |
| [M1_D_BUILD_RAW.txt](reference/audits/M1_D_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_C2_LOCAL_RAW.txt](reference/audits/M1_D_C2_LOCAL_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_CTEST_RAW.txt](reference/audits/M1_D_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_D1_RAW.txt](reference/audits/M1_D_D1_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_D2_RAW.txt](reference/audits/M1_D_D2_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_D4_RAW.txt](reference/audits/M1_D_D4_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_D6_RAW.txt](reference/audits/M1_D_D6_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_D7_CLOSE_RAW.txt](reference/audits/M1_D_D7_CLOSE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_D8_INJECTED_RAW.txt](reference/audits/M1_D_D8_INJECTED_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_GATES_REASSESSMENT_20261017.md](reference/audits/M1_D_GATES_REASSESSMENT_20261017.md) | Checkpoint gate reassessment/carry-forward; D06/D08 physical vẫn giữ HOLD. |
| [M1_D_STATIC_BUILD_RAW.txt](reference/audits/M1_D_STATIC_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M1_D_STATIC_CTEST_RAW.txt](reference/audits/M1_D_STATIC_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_A_BUILD_RAW.txt](reference/audits/M2_A_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_A_CTEST_RAW.txt](reference/audits/M2_A_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_B_BUILD_RAW.txt](reference/audits/M2_B_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_B_CTEST_RAW.txt](reference/audits/M2_B_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_B_HARDWARE_RAW.txt](reference/audits/M2_B_HARDWARE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_C_BUILD_RAW.txt](reference/audits/M2_C_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_C_CTEST_RAW.txt](reference/audits/M2_C_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_C_HARDWARE_RAW.txt](reference/audits/M2_C_HARDWARE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_C_WARP_RAW.txt](reference/audits/M2_C_WARP_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_D_BUILD_RAW.txt](reference/audits/M2_D_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_D_CTEST_RAW.txt](reference/audits/M2_D_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_D_HARDWARE_RAW.txt](reference/audits/M2_D_HARDWARE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_D_LIVE_HARDWARE_RAW.txt](reference/audits/M2_D_LIVE_HARDWARE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_D_LIVE_WARP_RAW.txt](reference/audits/M2_D_LIVE_WARP_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_D_WARP_RAW.txt](reference/audits/M2_D_WARP_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_LIVE2D_L0_PREFLIGHT_20261018.md](reference/audits/M2_LIVE2D_L0_PREFLIGHT_20261018.md) | Preflight header-only/SDK-path cũ; không dùng thay bằng native MOC evaluation/admission mới. |
| [M2_MOC3_F1_LOCAL_SOURCE.json](reference/audits/M2_MOC3_F1_LOCAL_SOURCE.json) | Manifest source/binary/dependency của lần chạy đã ghi; khác hash hiện tại không tự làm mất giá trị lịch sử. |
| [M2_MOC3_F1_PARTIAL_EVIDENCE.md](reference/audits/M2_MOC3_F1_PARTIAL_EVIDENCE.md) | Checkpoint partial đã được F1 local acceptance thay thế về trạng thái; vẫn là chứng cứ lịch sử. |
| [M2_MOC3_F2_LOCAL_SOURCE.json](reference/audits/M2_MOC3_F2_LOCAL_SOURCE.json) | Manifest source/binary/dependency của lần chạy đã ghi; khác hash hiện tại không tự làm mất giá trị lịch sử. |
| [M2_MOC3_F3_LOCAL_SOURCE.json](reference/audits/M2_MOC3_F3_LOCAL_SOURCE.json) | Manifest source/binary/dependency của lần chạy đã ghi; khác hash hiện tại không tự làm mất giá trị lịch sử. |
| [M2_MOC3_F4_COMPLETION_PROGRESS.md](reference/audits/M2_MOC3_F4_COMPLETION_PROGRESS.md) | Checkpoint controller/continuity/bundle/preflight trước static admission; đọc report mới cho PUB-01/PUB-03. |
| [M2_MOC3_F4_COMPLETION_SOURCE.json](reference/audits/M2_MOC3_F4_COMPLETION_SOURCE.json) | Manifest source/binary/dependency của lần chạy đã ghi; khác hash hiện tại không tự làm mất giá trị lịch sử. |
| [M2_MOC3_F4_GOAL_ASSESSMENT.json](reference/audits/M2_MOC3_F4_GOAL_ASSESSMENT.json) | Whole F4 REVISE còn phù hợp, nhưng next_action PUB-01 là checkpoint trước static slice mới. |
| [M2_MOC3_F4_LOCAL_SOURCE.json](reference/audits/M2_MOC3_F4_LOCAL_SOURCE.json) | Manifest source/binary/dependency của lần chạy đã ghi; khác hash hiện tại không tự làm mất giá trị lịch sử. |
| [M2_MOC3_F4_PACKAGE_ADMISSION_SOURCE.json](reference/audits/M2_MOC3_F4_PACKAGE_ADMISSION_SOURCE.json) | Manifest source/binary/dependency của lần chạy đã ghi; khác hash hiện tại không tự làm mất giá trị lịch sử. |
| [M2_PRE_D_BUILD_RAW.txt](reference/audits/M2_PRE_D_BUILD_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_PRE_D_CTEST_RAW.txt](reference/audits/M2_PRE_D_CTEST_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_PRE_D_HARDWARE_RAW.txt](reference/audits/M2_PRE_D_HARDWARE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_PRE_D_HOLD_REASSESSMENT_20261018.md](reference/audits/M2_PRE_D_HOLD_REASSESSMENT_20261018.md) | Checkpoint pre-D có raw witness; không phải ledger hiện tại cho toàn M2/F4. |
| [M2_PRE_D_LIVE_HARDWARE_RAW.txt](reference/audits/M2_PRE_D_LIVE_HARDWARE_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_PRE_D_LIVE_WARP_RAW.txt](reference/audits/M2_PRE_D_LIVE_WARP_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [M2_PRE_D_WARP_RAW.txt](reference/audits/M2_PRE_D_WARP_RAW.txt) | Raw log lịch sử gắn với report/source tương ứng; giữ nguyên byte/encoding/hash, không suy ra gate PASS mới. |
| [ARS_M1_SOLO_OWNER_ACK.md](reference/governance/ARS_M1_SOLO_OWNER_ACK.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |
| [ARS_M2_DEVELOPER.md](reference/governance/ARS_M2_DEVELOPER.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |
| [ARS_M3_FINAL_AUDIT.md](reference/governance/ARS_M3_FINAL_AUDIT.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |
| [ARS_M4_STACKED_PRS.md](reference/governance/ARS_M4_STACKED_PRS.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |
| [ARS_M5_MERGE_HISTORY.md](reference/governance/ARS_M5_MERGE_HISTORY.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |
| [ARS_M6_WAVE_STATE.md](reference/governance/ARS_M6_WAVE_STATE.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |
| [ARS_POLICY_V0_1_INDEX.md](reference/governance/ARS_POLICY_V0_1_INDEX.md) | Quy tắc governance local; header PENDING_MAIN/CANONICAL_IN_MAIN=NO là snapshot cũ: file và approved head có trong fetched origin/main. |

### Lỗi thời với vai trò kế hoạch hiện hành

| Tài liệu | Phạm vi / lý do / phần cần giữ |
| --- | --- |
| [M1_SOURCE_BOOTSTRAP_PLAN.md](obsolete/M1_SOURCE_BOOTSTRAP_PLAN.md) | Bootstrap/next-action sequence trước M1 đã qua; dùng scope và evidence từng phân đoạn. → [M1_A_SCOPE_V0.1.md](completed/M1_A_SCOPE_V0.1.md), [M1_A_BUILD_EVIDENCE.md](completed/audits/M1_A_BUILD_EVIDENCE.md), [M1_D_CLOSE_LOSS_NEXT_IMPLEMENTATION.md](in-progress/M1_D_CLOSE_LOSS_NEXT_IMPLEMENTATION.md) |
| [M2_LIVE2D_DYNAMIC_ADAPTER_PLAN.md](obsolete/M2_LIVE2D_DYNAMIC_ADAPTER_PLAN.md) | Phương án phụ thuộc official SDK đã được hoãn/thay bằng independent MOC3; giữ quyền sử dụng và điều kiện an toàn. → [M2_MOC3_INDEPENDENT_RUNTIME_PLAN.md](reference/M2_MOC3_INDEPENDENT_RUNTIME_PLAN.md), [M2_MOC3_FRAMEWORK_TARGET_DESIGN.md](in-progress/M2_MOC3_FRAMEWORK_TARGET_DESIGN.md) |
| [M2_MOC3_NEXT_IMPLEMENTATION.md](obsolete/M2_MOC3_NEXT_IMPLEMENTATION.md) | Không còn là danh sách “việc tiếp theo”: snapshot/host pixels/private physics đã triển khai; fidelity/public/loss nghĩa vụ vẫn còn. → [M2_MOC3_FRAMEWORK_TARGET_DESIGN.md](in-progress/M2_MOC3_FRAMEWORK_TARGET_DESIGN.md), [M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md](in-progress/M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md) |
| [V0_1_CANONICAL_CONTRACT_RC0.md](obsolete/V0_1_CANONICAL_CONTRACT_RC0.md) | RC0 được chính tài liệu đánh dấu superseded cho freeze decisions. → [V0_1_CANONICAL_CONTRACT_RC1.md](reference/V0_1_CANONICAL_CONTRACT_RC1.md) |

### Đang phát triển / triển khai / chờ kiểm chứng

| Tài liệu | Phạm vi / lý do / phần cần giữ |
| --- | --- |
| [M1_D_CLOSE_LOSS_NEXT_IMPLEMENTATION.md](in-progress/M1_D_CLOSE_LOSS_NEXT_IMPLEMENTATION.md) | Close/query private đã triển khai; host receipt/public qualification và D06/D08 còn thiếu. |
| [M1_D_GPU_CASE_EXECUTION.json](in-progress/M1_D_GPU_CASE_EXECUTION.json) | Ledger partial/private; D06/D08 thật tiếp tục HOLD, không có disposable runner. |
| [M2_D06_D08_LOSS_VERIFICATION_DESIGN.md](in-progress/M2_D06_D08_LOSS_VERIFICATION_DESIGN.md) | Kế hoạch loss thật chờ runner; fault injection local không thay kiểm thử vật lý. |
| [M2_D_HOLD_LEDGER_AND_SWITCH_DESIGN.md](in-progress/M2_D_HOLD_LEDGER_AND_SWITCH_DESIGN.md) | Private switching có chứng cứ; host seam/receipt/public gate và D3 loss thật còn HOLD. |
| [M2_MOC3_F4_PUBLIC_IMPORT_DESIGN.md](in-progress/M2_MOC3_F4_PUBLIC_IMPORT_DESIGN.md) | Public-input/isolation contract rộng hơn prototype và static admission đã hoàn tất. |
| [M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md](in-progress/M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md) | Kế hoạch hiện hành: PUB-02 retained evaluator, PUB-04 publication, PUB-05 SDK, PUB-06 qualification. |
| [M2_MOC3_F4_RELEASE_LEDGER.md](in-progress/M2_MOC3_F4_RELEASE_LEDGER.md) | Ledger hiện hành cho product gates, public F4/SDK/release và physical HOLD. |
| [M2_MOC3_FRAMEWORK_TARGET_DESIGN.md](in-progress/M2_MOC3_FRAMEWORK_TARGET_DESIGN.md) | Framework tổng thể: F1–F3/private F4 đã có; public F4/SDK/qualification chưa xong. |
| [M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md](in-progress/M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md) | Harness tiếp tục phát triển; CORE-LIVE đã có, đầu tài liệu còn HOLD cũ cần đọc cùng F4-B. |
| [M2_RENDERING_EXECUTION_PLAN.md](in-progress/M2_RENDERING_EXECUTION_PLAN.md) | Private A–D đã có source/evidence; G03/G04/G05 sản phẩm và loss thật chưa đóng. |
| [ROADMAP_AND_ADRS_V0.1.md](in-progress/ROADMAP_AND_ADRS_V0.1.md) | Roadmap M0–M4/ADR tổng thể; nhiều component đã có nhưng product/release chưa hoàn tất. |
| [V0_1_FORMAT_VALIDATION_FREEZE_MAP.md](in-progress/V0_1_FORMAT_VALIDATION_FREEZE_MAP.md) | Format/semantic freeze và public integration còn nghĩa vụ chưa được đóng. |
| [V0_1_PUBLIC_ABI_FREEZE_MAP.md](in-progress/V0_1_PUBLIC_ABI_FREEZE_MAP.md) | ABI20 declaration/compiler proof khác runtime/SDK consumer; public model/render còn unsupported. |
| [M1_B_CASE_EXECUTION.json](in-progress/audits/M1_B_CASE_EXECUTION.json) | CPU component evidence có; 87 whole-case PASS vẫn bằng 0 theo ledger này. |
| [M1_C_CASE_EXECUTION.json](in-progress/audits/M1_C_CASE_EXECUTION.json) | Private C2 được triển khai; full G02 và whole-case G08 vẫn HOLD. |

Bản đồ di chuyển: [DOCUMENTATION_PATHS.json](DOCUMENTATION_PATHS.json).

Di chuy?n th? m?c ?? qua review ??c l?p: 81 match workflow/document gi? nguy?n; governance 9/9 PASS; G00/RC1 checks PASS; contract suite 155/157 PASS v?i ??ng hai l?i M2-D ?? c? t? tr??c, kh?ng c? l?i m?i. Kh?ng suy ra whole-baseline PASS.

<!-- PUB02-A CHECKPOINT -->

## PUB-02-A implementation checkpoint

The classification baseline above has 125 files. This follow-on adds four records; the current exhaustive inventory has **129 files**: completed **29**, reference **80**, obsolete **4**, in-progress **16**. The original 121-file relocation map is a historical checkpoint; later document edits do not rewrite its hashes.

| Record | Category |
| --- | --- |
| [M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md](in-progress/M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md) | in_progress |
| [M2_MOC3_PUB02_A_ACCEPTANCE.md](completed/audits/M2_MOC3_PUB02_A_ACCEPTANCE.md) | completed |
| [M2_MOC3_PUB02_A_REVIEW.md](completed/audits/M2_MOC3_PUB02_A_REVIEW.md) | completed |
| [M2_MOC3_PUB02_A_SOURCE.json](reference/audits/M2_MOC3_PUB02_A_SOURCE.json) | reference |

Full PUB-02/PUB-04, isolated physics, public SDK and physical D06/D08 remain HOLD. Bounded parameter movement is not physics simulation.

<!-- PUB02-B CHECKPOINT -->

## PUB-02-B implementation checkpoint

Current exhaustive inventory: **133 files**; completed **31**, reference **81**, obsolete **4**, in-progress **17**. The four records added by this checkpoint are:

| Record | Category |
| --- | --- |
| [M2_MOC3_PUB02_B_ISOLATED_PHYSICS_DESIGN.md](in-progress/M2_MOC3_PUB02_B_ISOLATED_PHYSICS_DESIGN.md) | in_progress |
| [M2_MOC3_PUB02_B_ACCEPTANCE.md](completed/audits/M2_MOC3_PUB02_B_ACCEPTANCE.md) | completed |
| [M2_MOC3_PUB02_B_REVIEW.md](completed/audits/M2_MOC3_PUB02_B_REVIEW.md) | completed |
| [M2_MOC3_PUB02_B_SOURCE.json](reference/audits/M2_MOC3_PUB02_B_SOURCE.json) | reference |

Isolated physics is simulated in the bounded opt-in batch. Static and parameter-only modes still validate physics without simulation. Full persistent PUB-02/PUB-04, SDK and physical D06/D08 remain HOLD.

<!-- PUB04-A CHECKPOINT -->

## PUB-04-A implementation checkpoint

Current exhaustive inventory: **137 files**; completed **33**, reference **82**, obsolete **4**, in-progress **18**. New records:

| Record | Category |
| --- | --- |
| [M2_MOC3_PUB04_A_PUBLICATION_DESIGN.md](in-progress/M2_MOC3_PUB04_A_PUBLICATION_DESIGN.md) | in_progress |
| [M2_MOC3_PUB04_A_ACCEPTANCE.md](completed/audits/M2_MOC3_PUB04_A_ACCEPTANCE.md) | completed |
| [M2_MOC3_PUB04_A_REVIEW.md](completed/audits/M2_MOC3_PUB04_A_REVIEW.md) | completed |
| [M2_MOC3_PUB04_A_SOURCE.json](reference/audits/M2_MOC3_PUB04_A_SOURCE.json) | reference |

The bounded host now applies actual final core authorization after query completion and before target publication. Worker particle-state ACK, full PUB-04/PUB-02, SDK and physical D06/D08 remain HOLD. Earlier checkpoint counts and relocation hashes are historical.

<!-- PUB04-B CHECKPOINT -->

## PUB-04-B implementation checkpoint

Current exhaustive inventory: **141 files**; completed **35**, reference **83**, obsolete **4**, in-progress **19**. New records:

| Record | Category |
| --- | --- |
| [M2_MOC3_PUB04_B_CHECKPOINT_DESIGN.md](in-progress/M2_MOC3_PUB04_B_CHECKPOINT_DESIGN.md) | in_progress |
| [M2_MOC3_PUB04_B_ACCEPTANCE.md](completed/audits/M2_MOC3_PUB04_B_ACCEPTANCE.md) | completed |
| [M2_MOC3_PUB04_B_REVIEW.md](completed/audits/M2_MOC3_PUB04_B_REVIEW.md) | completed |
| [M2_MOC3_PUB04_B_SOURCE.json](reference/audits/M2_MOC3_PUB04_B_SOURCE.json) | reference |

Fresh isolated workers now restore the actual host-accepted particle/velocity/clock checkpoint and ACK its exact predecessor. Final core authorization accepts checkpoint and image together. This bounded bridge does not close persistent PUB-02/full PUB-04, shipping SDK or physical D06/D08. Earlier counts and relocation hashes are historical.

<!-- ALPHA CHECKPOINT -->

## Alpha integration and audit checkpoint (historical, before full #60)

Historical inventory before full #60: **160 files**; completed **46**, reference **84**, obsolete **4**, in-progress **26**. Follow [ALPHA_LINEAR_BACKLOG.md](in-progress/ALPHA_LINEAR_BACKLOG.md) and the [source-grounded audit](completed/audits/ALPHA_INTEGRATION_AND_ARCHITECTURE_AUDIT_20261019.md). Verified corrections: [issue52 test targets](completed/audits/ALPHA_52_NATIVE_TEST_TARGET_ACCEPTANCE.md), [issue53 custody](completed/audits/ALPHA_53_CUSTODY_ACCEPTANCE.md), [issue54 packed weights/UV](completed/audits/ALPHA_54_PACKED_ACCESSORS_ACCEPTANCE.md), [issue55 vertex color](completed/audits/ALPHA_55_VERTEX_COLOR_ACCEPTANCE.md), [issue56 D3D failure handling](completed/audits/ALPHA_56_D3D_FAILURE_ACCEPTANCE.md), [issue57 reflected transforms](completed/audits/ALPHA_57_REFLECTED_TRANSFORM_ACCEPTANCE.md). Issue58: [session design](in-progress/ALPHA_58_SESSION_LIFECYCLE_DESIGN.md), [wire/private API](in-progress/ALPHA_58_LEASE_PROTOCOL.md) and [R1-R5 direct local acceptance](completed/audits/ALPHA_58_SESSION_ACCEPTANCE.md). Issue59: [current ABI20 profile](in-progress/ALPHA_59_PUBLIC_ABI_CONTRACT.md), [development package usage](reference/SDK_DEVELOPMENT_PACKAGE.md) and [installed consumer acceptance](completed/audits/ALPHA_59_INSTALLED_ABI_ACCEPTANCE.md). Issue60: [isolated admission design](in-progress/ALPHA_60_ISOLATED_ADMISSION_DESIGN.md) and [A60-1a local acceptance](completed/audits/ALPHA_60_ISOLATED_ADMISSION_ACCEPTANCE.md). [Typed2D transfer design](in-progress/ALPHA_60_TYPED_2D_TRANSFER_DESIGN.md) and [local acceptance](completed/audits/ALPHA_60_TYPED_2D_TRANSFER_ACCEPTANCE.md) cover the next private substep. At that checkpoint complete GLB/dual transfer, CPU ownership and public authority were still OPEN; capabilities described the default feature-off profile. The latest #60 checkpoint below supersedes that implementation status for the explicit CPU profile. PR44's [host-handoff plan](in-progress/M2_E_HOST_HANDOFF_PLAN.md) is preserved. Earlier counts/source pins are historical; integration is not release qualification.


## #60 public CPU completion - 21-10-2026

Current exhaustive inventory: **163 files**; completed **47**, reference **84**, obsolete **4**, in-progress **28**. [Design](in-progress/ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md), [continuation ledger](in-progress/ALPHA_60_COMPLETION_PROGRESS.md) and [direct local acceptance](completed/audits/ALPHA_60_PUBLIC_CPU_ACCEPTANCE.md) reconcile full typed native2D/GLB/dual, bounded runtime ownership, atomic public authority publication and installed four-config CPU SDK qualification. Final committed custody/integration is recorded on issue60/its PR. The earlier quota blocker is resolved. Default feature-off behavior and physical D06/D08 HOLD remain separate; CPU integration is not release qualification.
