# Interactive Avatar Studio

Owner request (2026-10-22): implement a fuller, more attractive interactive
desktop UI like launch_interactive_viewer.bat, with more controls and automatic
effects for direct human testing. This is authorized implementation, separate
from the completed #39 and next #41 gate qualification.

Requirements UI1..UI6: visible resizable viewport and clear control hierarchy;
smooth expression presets and configurable idle/blink/breath/gaze/speech/wind;
mouse pull/pan/zoom plus searchable actual parameters and part opacity;
linked Physics3 using the existing retained solver; local presets, reset and
clean PNG captures; robust launch, visible smoke proof and independent review.

Use a standalone raylib desktop app with the locally available MIT Purism Core
renderer modules. Preserve the owner's viewer.c/viewer.h edits and the existing
diagnostic launcher path. Default launcher selects one authorized local model
or opens a file picker. No private model/texture/frame is tracked or uploaded.
No dependency downloads, official SDK, public ABI/profile changes, driver reset,
or public activation. Physics absence/failure is visible; no synthetic spring
is presented as linked Physics3. This diagnostic viewer is not ARS host-render
acceptance or reference-equivalence proof.

Layout: dark header and live status, left motion/scene controls, central model
stage with camera toolbar and performance timeline, right searchable parameter/
part inspector. Pause/step, expression cycle, background/grid/particles,
camera presets and model replacement remain directly accessible. Sliders show
values; pinned manual values override automatic motion and are clamped to the
actual model catalog. File loads and presets validate before changing state.

Build separately under ignored build/avatar-studio, using existing local source
trees and caches. Reuse retained Physics3 without changing its bounded lifecycle:
rebase accepted state after eight updates; all solver/render calls stay on the
window thread. All outputs/settings are local under ignored build/studio-user.

Verify finite/clamped motion and settings, preset rollback, repeated physics
epoch continuity, real visible native frames, controls/pause/camera/resize,
PNG content and clean close. Keep local visual evidence private. Existing
diagnostic guard tests remain unchanged. Source review and delivery distinguish
actual observations from unrun or hardware-specific checks.

## Local implementation checkpoint — 2026-10-22

Implemented on `dev/avatar-interactive-studio`, based on alpha
`006f36689d1c432659925ea2f9abe865a7825831`. Entry point:
[launch_interactive_viewer.bat](../../launch_interactive_viewer.bat).
Usage and limitations: [Studio README](../../tools/avatar-studio/README.md).
No public model/texture/image is added. Existing diagnostic helper and vendor
viewer edits are preserved; the new viewer builds separately.

| Requirement | Direct local verification | Result |
| --- | --- | --- |
| UI1 viewport/layout | Actual native window and inspected full-window screenshots at 1460×1000 and 1250×920; nonblank model | PASS |
| UI2 automatic controls | Idle/happy/dance/zoom captures differ by 37,248 / 55,390 / 92,548 pixels above threshold 25 from idle on permitted fixture; native finite/catalog bounds monitored | PASS within this fixture |
| UI3 manual/camera | Real keyboard and mouse: expression, search/pin/right-click clear, layers tab, pause/step, face/fit, background | PASS |
| UI4 linked physics | Native 80-update boundary probe at 125ms/update; 1800 visible frames, linked solver remains available; six native checks, zero failures; final 60 FPS observed | PASS within this fixture |
| UI5 persistence/capture | Actual Save/Load, corrupt preset rollback, PNG export, clean close; preset/state replacement uses temporary file plus atomic Windows replacement | PASS |
| UI6 launch/regression/review | Auto-selected local model, separate output folders; old diagnostic guards 4/4 and linked-physics Python tests 9/9; independent reviewer identified boundary defect, fixed with direct RED/GREEN and reviewed again | PASS |

Actual commands: `python scripts/avatar_studio.py --smoke --frames 1800`;
`ARS_STUDIO_LIVE=1` with unittest `test_avatar_studio_live.py` (six behavior
groups in one integration test, 12.2 seconds); existing unittest files
`test_viewer_diagnostic_guards.py` and `test_linked_physics.py`.
Private evidence stays under `build/studio-user/`; logs under `build/`.
First full-window screenshot test exposed raylib absolute-path screenshot
behavior; export now uses an explicit screen image. GUI harness was corrected
to wait for cursor event processing, PNG completion and owned-window focus.
The final successful test kept all assertions and closed its owned process.

Reviewer P2: at max speed and slow frames, elapsed plus retained remainder could
exceed the solver's eight-step bound and disable physics. The same native
80-update probe failed before the adapter fix and passed afterwards. Adapter
now splits elapsed against remaining step capacity, preserving elapsed time,
and rebases accepted epochs. Public runtime code was not changed.

Independent review provenance: separate reviewer context
`/root/f2_independent_review`, no reviewer-authored source edits. Final scoped
verdict PASS: prior P2 fixed; no further supported P0–P2, architecture and project
boundaries coherent. Reviewer read existing runtime receipts, did not run new
workloads. Final coder reconciliation confirms the native sources were unchanged
after the 1800-frame and real-controls runs; later launcher adjustment excludes
README changes from source-freshness rebuild detection and includes ARS headers.
Exact final file hashes and local evidence pointers are retained in
`build/studio-user/verification-source.json`.

Not claimed: arbitrary-model visual equivalence, author motion playback,
microphone lip-sync, high-DPI or multi-monitor qualification, all GPU/model
versions, public SDK acceptance, G01/G02 or physical D06/D08 closure. Human
hands-on acceptance remains with the owner. This document stays in-progress
for that iteration; implemented local behavior above is independently testable.
