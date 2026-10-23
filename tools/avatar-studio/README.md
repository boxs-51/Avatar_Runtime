# Avatar Studio

Giao diá»‡n desktop Ä‘á»ƒ quan sÃ¡t vÃ  chá»‰nh model local theo thá»i gian thá»±c.
Tá»« thÆ° má»¥c repo, cháº¡y `launch_interactive_viewer.bat` hoáº·c:

```powershell
python scripts/avatar_studio.py
python scripts/avatar_studio.py --model PATH_TO_LOCAL_MODEL3_JSON
```

Náº¿u cÃ³ Ä‘Ãºng má»™t model trong thÆ° má»¥c model cá»¥c bá»™ Ä‘Ã£ cáº¥u hÃ¬nh, Studio tá»± chá»n model Ä‘Ã³; náº¿u khÃ´ng,
Studio má»Ÿ há»™p chá»n file. `MOC3_VIEWER_PROFILE` hiá»‡n cÃ³ váº«n Ä‘Æ°á»£c Æ°u tiÃªn. NÃºt
**Open model** thay model trong cá»­a sá»•; model lá»—i giá»¯ nguyÃªn model Ä‘ang cháº¡y.

- ChÃ­n biá»ƒu cáº£m chuyá»ƒn tiáº¿p mÆ°á»£t; tá»± Ä‘á»•i biá»ƒu cáº£m, chá»›p máº¯t, thá»Ÿ, nhÃ¬n theo
  chuá»™t, nÃ³i mÃ´ phá»ng, Ä‘iá»u chá»‰nh tá»‘c Ä‘á»™/cÆ°á»ng Ä‘á»™/chuyá»ƒn Ä‘á»™ng giÃ³.
- Physics3 liÃªn káº¿t vá»›i model cháº¡y báº±ng retained solver hiá»‡n cÃ³. Tráº¡ng thÃ¡i
  thiáº¿u hoáº·c lá»—i physics hiá»‡n rÃµ; khÃ´ng tá»± thay báº±ng spring giáº£.
- KÃ©o chuá»™t trÃ¡i Ä‘á»ƒ Ä‘iá»u khiá»ƒn Ä‘áº§u/tay/thÃ¢n; chuá»™t pháº£i/giá»¯a Ä‘á»ƒ pan; cuá»™n Ä‘á»ƒ zoom.
  Fit/Face Ä‘áº·t camera, Reset khÃ´i phá»¥c scene vÃ  cÃ¡c giÃ¡ trá»‹ chá»‰nh tay.
- Inspector tÃ¬m theo ID, chá»‰nh vÃ  ghim tham sá»‘, chá»‰nh Ä‘á»™ hiá»‡n cá»§a part/layer.
  Chuá»™t pháº£i trÃªn hÃ ng Ä‘Ã£ ghim Ä‘á»ƒ bá» ghim. Clear manual overrides khÃ´i phá»¥c
  tham sá»‘ vÃ  opacity gá»‘c. Tham sá»‘ model khÃ´ng cÃ³ sáº½ khÃ´ng Ä‘Æ°á»£c tá»± táº¡o.
- Space pause/play; Step tiáº¿n má»™t frame mÃ´ phá»ng; P báº­t/táº¯t physics; 1â€“9 chá»n
  biá»ƒu cáº£m; R fit camera; F12 hoáº·c Capture lÆ°u PNG viewport.
- Bá»‘n ná»n, grid, particles, biá»ƒu Ä‘á»“ thá»i gian frame; Save/Load preset giá»¯ cÃ¡c
  tÃ¹y chá»‰nh vÃ  override. Preset sai model hoáº·c há»ng bá»‹ tá»« chá»‘i.

Preset, `state.json` vÃ  PNG chá»‰ lÆ°u trong **`build/studio-user/`**, Ä‘Æ°á»£c Git bá»
qua. Preset giá»¯ zoom nhÆ°ng khÃ´ng lÆ°u vá»‹ trÃ­ pan. Capture/F12 ghi Ä‘Ã¨
`viewport.png`; hÃ£y sao chÃ©p áº£nh cáº§n giá»¯ trÆ°á»›c láº§n chá»¥p káº¿ tiáº¿p. CÃ¡c Ä‘iá»u khiá»ƒn
chu ká»³ bá»• sung xuáº¥t hiá»‡n khi cá»­a sá»• Ä‘á»§ cao. Giao diá»‡n dÃ¹ng tiáº¿ng Anh.

`--legacy` má»Ÿ viewer cÅ©. `--build` yÃªu cáº§u build tÄƒng dáº§n. Studio tá»± build khi
chÆ°a cÃ³ binary hoáº·c source Studio/runtime má»›i hÆ¡n binary. Cáº§n Windows x64,
CMake vÃ  Visual Studio C++; dÃ¹ng source Purism Core/raylib/utf8proc Ä‘Ã£ cÃ³ trong
`build/`. KhÃ´ng táº£i dependency hay SDK má»›i. Má»™t checkout má»›i chÆ°a cÃ³ cÃ¡c source
local nÃ y cáº§n chuáº©n bá»‹ dependency trÆ°á»›c; launcher sáº½ bÃ¡o lá»—i rÃµ.

```powershell
python scripts/avatar_studio.py --smoke --frames 1800
$env:ARS_STUDIO_LIVE='1'
python -m unittest discover -s tests/moc3 -p test_avatar_studio_live.py -v
```

Smoke má»Ÿ cá»­a sá»• tháº­t, kiá»ƒm tra physics á»Ÿ biÃªn tá»‘c Ä‘á»™, pause, preset, resize vÃ 
lÆ°u áº£nh vÃ o thÆ° má»¥c riÃªng. BÃ i test GUI gá»­i phÃ­m/chuá»™t chá»‰ khi Ä‘Ãºng cá»­a sá»• do nÃ³
má»Ÿ Ä‘ang á»Ÿ foreground, rá»“i Ä‘Ã³ng process Ä‘Ã³. Khi cháº¡y bÃ i nÃ y, Ä‘á»ƒ chuá»™t/bÃ n phÃ­m
ráº£nh cho test. Cháº¡y thÆ°á»ng khÃ´ng gá»­i input tá»± Ä‘á»™ng.

ÄÃ¢y lÃ  viewer cháº©n Ä‘oÃ¡n **trusted-local** dÃ¹ng Purism Core/sample OpenGL renderer
(MIT, Sakura Motion Project) vÃ  retained Physics3 cá»§a repo. Chá»‰ má»Ÿ model tin cáº­y.
Biá»ƒu cáº£m, nÃ³i vÃ  giÃ³ lÃ  procedural controls, khÃ´ng pháº£i phÃ¡t motion/expression
do tÃ¡c giáº£ model cung cáº¥p hay nháº­n Ã¢m thanh tá»« micro. Káº¿t quáº£ khÃ´ng thay chá»©ng
cá»© ARS host/D3D11, khÃ´ng chá»©ng minh tÆ°Æ¡ng Ä‘Æ°Æ¡ng Cubism vÃ  khÃ´ng Ä‘Ã³ng gate sáº£n pháº©m.

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

## Local per-drawable rig editing â€” cache and Agent query
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
That role is stored with the local rig in STUDIO_RIG_4 format, with
backward-compatible read of STUDIO_RIG_1. A per-drawable rig cache keeps
the manual label while switching layers.

Authorized local Agent API v1.0 additions:
- \`layer.scan\` â€” grouped numeric totals (no names or vertex positions);
  \`up/mid/low/span\` are relative bounding-box regions and \`bad\` is
  the number of drawables that could not be evaluated.
- \`layer.suggest\` â€” selected drawable's role hint, confidence, proposed
  anchor count, region and shape, with no raw identifier.
- \`layer.adopt\` â€” explicit acceptance of high-confidence suggested roots;
  refuses ambiguous candidates, does not enable physics automatically.
- \`layer.label\` â€” explicit manual assignment: kind is exactly
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
