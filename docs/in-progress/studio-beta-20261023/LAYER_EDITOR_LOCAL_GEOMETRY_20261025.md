# Native Layer Editor — local geometry authoring checkpoint

Date: 2026-10-25; beta branch exp/g5-local-cas-transactions. All changes local, uncommitted. User intent: build usable create/edit/delete layer editor, test real GUI against correct Studio window.

## Implemented

- Native owner-thread geometry document separate from compiled MOC3: 8 custom sublayers, up to 128 vertices and 256 triangles per layer, six in-memory undo snapshots.
- Create/delete/duplicate/reorder custom layers; add/move/delete points; add/delete triangles; copy a bounded MOC3 drawable into an editable sidecar geometry layer; undo/redo; save/reopen.
- Layers tab: mouse point creation and selection; point position adjustment; face creation/removal; authored-layer navigation, duplicate/order, undo/redo, import, private save/load. All custom layers draw as overlays in document order, with a selected layer highlighted; custom-only preview hides compiled geometry without mutating it.
- Existing Local Agent V1 IPC accepts layer.project.status/save/load, layer.create, layer.copy.drawable, layer.delete/duplicate/reorder, layer.active, layer.point.add/move/delete, layer.face.add/delete, layer.edit.undo/redo.
- Generation/revision CAS, unique layer IDs, finite bounded coordinates, triangle connectivity, capacity and referenced-vertex deletion guards. Invalid commands preserve prior state.
- Fixed local sidecar filenames and best-effort backup. Saved project is bound to the content fingerprint of the private compiled model and Studio identity. No model data is published.

## Verified

- Synthetic Native C create/edit/delete/import/duplicate/reorder, undo/redo, save/load and invalid-input tests PASS.
- Real Native Studio API end-to-end: edit, delete, undo/redo, save/load, duplicate/reorder, stale denial, source MOC3 unchanged PASS.
- Separate restart test: persisted geometry reload PASS; old generation and corrupted file rejected without mutating current editor state.
- Eligible MOC3 drawable imported as sidecar geometry copy PASS; compiled model unchanged.
- GUI script launches isolated Studio and checks title, exact PID/HWND, foreground and window at click point before every input. Create layer, 3 mouse vertices, triangle, delete, undo and save PASS on target window.
- Final broad regression evidence recorded separately.

## Explicit limitations

The new custom-layer geometry editor is functional but is **not** a complete native MOC3 source editor. Imported source geometry is a snapshot; original compiled geometry and UV, textures, material, deformation/skin rig are NOT modified. No source-model re-export, GPU/XPBD binding for custom geometry, unlimited history or independent visual usability review is claimed. Address these gaps through the product Layer/Model Editor workflow without reopening standalone G5 infrastructure.

Local-only; do not commit, push, merge, or publish model data.


## Latest verification and integration notes

- Real Native Studio Release build PASS. Current selected Native suite: **17/17
  PASS**; 3 heavy multi-process fixtures intentionally excluded from this run.
- Full Python moc3 and publication guard: **310 PASS, 19 SKIP, 229 subtests
  PASS**. Local Agent API source and contract tests included.
- Physical product E2E PASS for create/edit/delete/duplicate/reorder, point/face
  readback, undo/redo and sidecar save/load. Restart + malformed-file regression
  PASS, bounded compiled drawable import PASS.
- PID-fenced GUI test PASS again on the current binary. The runner creates
  its own Studio process, verifies exact HWND, unique PID/title, foreground
  and window-at-cursor just before each down event, and restores the previous
  foreground after test. No other user application receives scripted clicks.
- Custom authored layers render as overlays regardless of inspector tab;
  interaction is enabled only in the geometry editing mode. Source MOC3
  buffers remain immutable.
- Remaining HOLD: editing original MOC3 source geometry/UV/materials,
  authored-layer deformation/XPBD binding, texture draw-order integration,
  independent screenshot-based visual QA, crash-durable backup guarantees
  and independent code review. Therefore not a completed MOC3 source editor.
