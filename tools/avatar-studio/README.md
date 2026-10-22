# Avatar Studio

Giao diện desktop để quan sát và chỉnh model local theo thời gian thực.
Từ thư mục repo, chạy `launch_interactive_viewer.bat` hoặc:

```powershell
python scripts/avatar_studio.py
python scripts/avatar_studio.py --model PATH_TO_LOCAL_MODEL3_JSON
```

Nếu có đúng một model trong thư mục model cục bộ đã cấu hình, Studio tự chọn model đó; nếu không,
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

## Primary Layer Physics Studio entrypoint (local experiment)

On branch exp/layer-physics-native-studio-local, `launch_layer_physics_studio.bat`
is the primary launch shortcut. `launch_interactive_viewer.bat` is retained
as a compatible alias. Both route via `scripts/layer_physics_studio.py` into
the existing native raylib `tools/avatar-studio` implementation
(`scripts/avatar_studio.py` builds/launches `build/avatar-studio/Release/avatar_studio.exe`).

The original PurismCore viewer remains available by explicit `--legacy`.
This branch does not claim native drawable XPBD integration: current physics
is the existing linked Physics3 backend. The renderer and UI stay local; PNGs,
presets and diagnostics live under ignored `build/studio-user/`.
No Git remote writes, uploads, model, texture or log exports are authorized.

Entry-point regression:
`py -m unittest discover -s tests/moc3 -p test_layer_physics_native_entrypoint.py -v`

## Experimental native XPBD (local only)
The Native Studio XPBD tab now uses `studio_xpbd.cpp` and the locally
reused `native_mesh_xpbd.hpp` solver. Pick one or two drawable indices
(total <= 2048 vertices), adjust gravity/wind/compliance/collision, optionally
connect the two meshes, then Apply XPBD. Disable restores original renderer
positions; no MOC3 or texture bytes are modified. Existing linked Physics3
continues to drive the base pose before XPBD.

The renderer substitutes XPBD vertex pointers only for a prepared frame.
After native raylib rendering, it flushes the OpenGL batch and waits for
completion before accepting XPBD state. A failed solver/render check stops
XPBD and restores original geometry; it never publishes a failed candidate.

On this local experiment branch:
- `py -m unittest discover -s tests/moc3 -p test_studio_xpbd_contract.py -v`
- `cmake --build build/avatar-studio --config Release --target avatar_studio`
- Direct opt-in test: `build/avatar-studio/Release/avatar_studio.exe --model <local.model3.json> --frames 125 --xpbd-smoke`
- Primary GUI launcher: `launch_layer_physics_studio.bat`

`--xpbd-smoke` is a local, finite-run probe for one eligible drawable;
it does not auto-enable XPBD in the normal interactive launcher.
No local model, texture, screenshot or log is published remotely.

## Extended XPBD controls and automatic motion experiments (local branch)
The XPBD inspector now separates Forces and Advanced settings. Forces: gravity,
wind, edge compliance, collision radius and optional two-mesh joint. Advanced:
per-vertex mass, velocity damping (0..1), contact compliance, solver iterations
(1..24), step duration (1/240..1/60) and pin vertex (0..100 in GUI).
All settings are applied on Apply XPBD by re-binding the solver; previous state
is preserved if validation rejects a new setting. Pin index must exist on each
selected drawable; the solver refuses invalid/budget-exceeding selections.

The native solver has synthetic deterministic parameter oracles, independent of
private model files:
  cmake --build build/avatar-studio --config Release --target studio_xpbd_parameter_tests
  build/avatar-studio/Release/studio_xpbd_parameter_tests.exe

Physical motion sequence: 3 independently launched model sessions (125 frames
each), parameter changes at frame 44 and 74, and assertions of accepted XPBD
frames, zero native failures, correct stop/disable:
  py scripts/test_xpbd_motion_matrix.py

Run basic physical XPBD regression:
  py scripts/test_native_xpbd.py

All PNG, model, logs and summaries stay within local ignored build/studio-user.
Current physics motion scripts prove bounded, accepted execution and successful
re-bind; they do not yet quantify vertex displacement for all mesh topologies.

## Experimental advanced XPBD: bending, region anchors and surface exclusion

The local native solver supports an approximate 2D bending constraint:
opposite vertices in adjacent triangles are connected by a compliant
distance constraint. Region anchors can pin up to eight consecutive vertex
indices per selected drawable (currently derived from the GUI's Pin Vertex).
The contact solver projects dynamic vertices outside filled triangles of
other selected drawables using an optional surface thickness. This is
**discrete 2D vertex-triangle exclusion**, not swept continuous collision,
triangle-triangle intersection handling, self-collision, or 3D cloth physics.

The public Studio binding adds bending compliance, surface thickness and
anchor count to StudioXpbdOptions. Defaults keep the existing XPBD smoke
and motion-matrix tests runnable.

Synthetic RED-first oracles in native_xpbd_parameter_tests.cpp:
- hair-like two-anchor mesh with bending enabled
- skirt-like dynamic mesh against static triangle, testing no interior point
- lag / overshoot / settling after source pose moves
- stale and invalid-parameter cases

Commands:
  cmake --build build/avatar-studio --config Release --target avatar_studio studio_xpbd_parameter_tests
  build/avatar-studio/Release/studio_xpbd_parameter_tests.exe
  py scripts/test_xpbd_motion_matrix.py

Limitations: no semantic hair/skirt auto classification, anchor map authoring,
continuous collision detection, bend angle constraints or visual pixel oracle.
The two selected meshes only are admitted into the solver, so physical contact
with arbitrary unselected body layers is not modeled yet.

## 2026-10-23 local quality regression: zoom / simulated motion
- Super-sampled model viewport: `STUDIO_RENDER_SCALE=1.5`; viewport is
  resolved through the existing linear texture filter. This is a raster
  quality improvement, not a guarantee that every MOC3 texture is sharp.
- XPBD default transfer=.03, damping=.96, step=1/60 and substeps=4.
  These are experimental defaults and can be overridden in Studio.
- Post-XPBD display smoothing uses a candidate `MotionFilter` with an
  exponential response (85 ms), separate from accepted solver state.
  Its own candidate is accepted only after the solver frame is accepted;
  it is not used to hide failed native validation.
- Synthetic jerk-regression: filtered=2.17667, original=21.06,
  ratio=0.103356 on an alternating 0.045-unit jitter input.
  This is synthetic numerical evidence, not a measured visual FPS guarantee.
- Windows private fixture run: 3 motion cases PASS; 125-frame native XPBD
  PASS; 280-frame visible smoke (including window resize) PASS with 7
  checks and zero failures.
- Known remaining: frame pacing and rendered hair/skirt vertex trajectories
  on the actual user model need independent quantitative observations.

## Local experimental Layer Rig Editor (2026-10-23)
The Native Studio right inspector now includes a **Rig** tab alongside
Params/Layers/XPBD. Select a drawable and vertex index, toggle multiple
anchors and mark the two endpoints of a support beam. Save / Load writes
`build/studio-user/rig-local.txt` (ignored workspace data), and rejects
mismatched stored model signatures/vertex count. **Apply rig to XPBD**
submits anchor vertices and compliant same-drawable support beams to
`studio_xpbd_bind_rig`. This does not modify original MOC3 files.

Implementation: `studio_rig.{h,c}`, `studio_xpbd.cpp`,
`native_mesh_xpbd.hpp`, `studio.c`. Runtime test:
`build/avatar-studio/Release/studio_rig_tests.exe`.
These checks use explicit runtime checks in Release; no assertions compiled
out. Native Studio, synthetic XPBD, and physical 125-frame XPBD smoke passed.

Limitations of this first editor: vertices are selected by index through
on-screen controls, NOT by direct click on rendered geometry; beam lines,
bone hierarchy and weighted skinning are NOT yet implemented. The rig
is scoped to one drawable and one local preset. User drag/rotate/release
have not been mapped to authored action profiles or separately measured.
Rig signature uses parameter and part identity; it does not yet include
a full drawable topology fingerprint. This is a foundation rather than
a completed hair/skirt physics authoring system.

## Rig viewport controls (experimental native UI)
Open the **Rig** tab in the right inspector. Choose drawable with +/-,
then click a vertex in the model viewport to select it. Shift+click toggles
an anchor and Ctrl+click the two beam endpoints creates/removes a support
beam. The overlay draws selected/anchored points and support beams. While
Rig is selected, left mouse no longer triggers the ordinary model-pull
interaction. Save / Load is local only under `build/studio-user`.
**Apply rig to XPBD** binds these vertices and support beams; editing
the rig afterward requires another Apply.

These are per-drawable mesh attachment constraints, not a full
skeletal hierarchy, parent-child bones, automatic weight painting or
interaction-specific physical action presets. The no-crash physical
125-frame model smoke and native regression tests pass; viewport
mouse selection still needs an explicit end-to-end interaction test.

## Local public Agent API v1.0 (opt-in Studio session)
Agent commands are exposed through Python module
\`scripts/studio_agent_api.py\` and JSON command CLI
\`scripts/studio_agent_cli.py\`. The interface is public to agent callers
running in the **same authorized local workspace**. It is NOT a public
Internet HTTP endpoint; no listening port, cloud upload or external service.
The bridge uses bounded one-request-at-a-time ASCII files under the ignored
\`build/studio-user/api/\` directory. A real Studio must be open; a call
without a native acknowledgement raises TimeoutError, not a false PASS.

Launch the GUI normally:
\`launch_layer_physics_studio.bat\`

Invoke commands from another local terminal:
\`py scripts/studio_agent_cli.py '{"op":"rig.select","drawable":0}'\`
\`py scripts/studio_agent_cli.py '{"op":"rig.anchor","vertex":2,"enabled":true}'\`
\`py scripts/studio_agent_cli.py '{"op":"rig.beam","a":3,"b":9,"enabled":true}'\`
\`py scripts/studio_agent_cli.py '{"op":"physics.set","gravity":-12,"wind":4,"damping":0.95}'\`
\`py scripts/studio_agent_cli.py '{"op":"action.trigger","kind":"pull","strength":0.5}'\`
\`py scripts/studio_agent_cli.py '{"op":"rig.apply"}'\`

Supported operations: \`rig.select\`, \`rig.anchor\`, \`rig.beam\`,
\`rig.apply\`, \`physics.set\`, and \`action.trigger\` with one of
pull/release/shake/settle. Index bounds and numeric ranges are checked.
A successful receipt confirms the Studio processed a command, but is NOT
a physics-quality certification. \`physics.set\` updates pending controls:
apply rig to rebind XPBD with those parameters.

Known limitations: single in-flight agent; file bridge is writable by
the same OS account and is not an authorization boundary; do not expose the
workspace's api folder to untrusted users. No model data/texture export.
Version v1.0 provides no layer-list query, topology fingerprint,
asynchronous events or independent action-physics profile engine yet.
Tests: \`py -m unittest discover -s tests/moc3 -p test_studio_agent_api.py\`
and \`py scripts/test_studio_agent_api_live.py\`.

## 2026-10-23 real-model interactive metrics gate
Agent API live sequence now records stage snapshots locally, not just receipts:
\`py scripts/test_studio_interaction_metrics.py\`
Stages rig.select / 2 anchors / beam / rig.apply / pull / shake / release /
settle completed at frames 15..135, with 74 accepted XPBD frames and
reported pin drift zero. Metrics are aggregate maxima since XPBD binding;
on the tested model, cumulative max step and acceleration did not increase
after pull/shake. This is **REVISE** for behavioral quality: stage-local
motion response, displacement amplitude, settling duration and semantic
hair/skirt mapping remain unproven. API acknowledgement is not a physics
quality oracle. Log/JSON only in ignored build/studio-user.
Telemetry regression fixed moved-from candidate access before numerical
read; pinned presentation points bypass smoothing.

## Local per-drawable rig editing — cache and Agent query
The Rig editor now retains distinct anchor/beam sets for up to
32 drawables per model generation. Switching A -> B -> A through the
GUI or local Agent API restores A's authored constraints; changing
the model signature invalidates the old cache. Per-drawable saved
presets use ignored rig-layer-N.txt files (no model bytes written).
If the bounded bank is full, a new uncached outgoing rig is not
silently discarded.

Agent caller may invoke rig.inspect with no other fields to obtain
only the selected drawable index, anchor count and beam count.
These numeric receipts do not enumerate model parts or export
textures/geometry. Model-private local E2E: 11 agent operations PASS
including editing A, switching to B and back, inspecting A, then
reapplying the authored rig. Native synthetic rig cache tests PASS.

## Preliminary drawable decomposition and agent-assisted labeling

The **Rig** tab includes an opt-in **Analyze layer** action. It reads the
currently loaded MOC3 drawable *locally*, evaluates conservative semantic
name hints (hair, skirt, ribbon/strap) and mesh geometry, then previews
confidence and candidate root vertices. The model file is not changed.
Ambiguous identifiers remain **unknown** and cannot automatically adopt
anchors. For unknown layers, positional grouping (upper, middle, lower,
spanning) and mesh shape are only geometry hints, NOT verified body-part
labels.

**Adopt anchors** is separately gated to a known semantic class with
confidence >=80, and changes the editable rig only; **Apply rig to XPBD**
remains a separate deliberate action. Users can explicitly confirm a
drawable's role as Unknown, Hair, Skirt or Strap using the rig controls.
That role is stored with the local rig in STUDIO_RIG_2 format, with
backward-compatible read of STUDIO_RIG_1. A per-drawable rig cache keeps
the manual label while switching layers.

Authorized local Agent API v1.0 additions:
- \`layer.scan\` — grouped numeric totals (no names or vertex positions);
  \`up/mid/low/span\` are relative bounding-box regions and \`bad\` is
  the number of drawables that could not be evaluated.
- \`layer.suggest\` — selected drawable's role hint, confidence, proposed
  anchor count, region and shape, with no raw identifier.
- \`layer.adopt\` — explicit acceptance of high-confidence suggested roots;
  refuses ambiguous candidates, does not enable physics automatically.
- \`layer.label\` — explicit manual assignment: kind is exactly
  \`unknown\`, \`hair\`, \`skirt\` or \`strap\`. It is not an automatic
  classifier and can be revised before saving the rig.
- \`rig.inspect\` additionally reports the manual role.

Example CLI (inside the trusted workspace):
\`py scripts/studio_agent_cli.py '{"op":"layer.scan"}'\`
\`py scripts/studio_agent_cli.py '{"op":"layer.suggest"}'\`
\`py scripts/studio_agent_cli.py '{"op":"layer.label","kind":"hair"}'\`

RED-first synthetic classification tests:
\`build/avatar-studio/Release/studio_layer_analysis_tests.exe\`
Live-model privacy-preserving numeric regression:
\`py scripts/test_studio_layer_scan_live.py\`
Performance timing is measured *after Studio readiness*; time spent
starting the model is not counted as classification latency.

This is preliminary spatial decomposition, NOT automatic physical
segmentation or verified hair/skirt detection. It does not export
private model names, coordinates, screenshots or textures. The local
file bridge is limited to trusted processes sharing the Windows account;
it provides no separate OS-enforced authorization boundary.
