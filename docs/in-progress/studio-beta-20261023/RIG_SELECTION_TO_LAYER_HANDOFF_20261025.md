# Rig selection → Layer Editor handoff (2026-10-25)

Contract: AVATAR-STUDIO-LOCAL-001/C0; branch exp/g5-local-cas-transactions, local worktree, no commit or push.

## Implemented product scope

- Owner-thread RigBank selection now validates model signature, drawable index and actual vertex count before changing Rig, revision or selected UI state.
- Rig UI next/previous skips empty and oversized meshes and remains navigable when the XPBD selector points at an unsupported drawable. The viewport Rig overlay also avoids unsupported geometry.
- Agent commands rig.selection(generation), rig.select.at(generation,revision,drawable), layer.select(generation,revision,drawable), layer.inspect(generation) use the existing bounded V1 IPC. Stale model and revision, invalid targets and extra wire bytes are rejected before live mutation. The old rig.select is compatible.
- Layer inspection is an aggregate/local-private read of selected drawable, vertex count, triangles, topology validity, islands, loose components and Rig revision. No raw geometry, model names or texture data in receipts.
- Layer inspector tab has a read-only layer/drawable selector and still shows the existing Part opacity controls. This is NOT a mesh editor.
- After layer.select, Studio status confirms inspector tab=1; after rig.select.at, tab=3. Both routes restore RigBank editing state and use one selected drawable, not an independent Agent layer model.

## Evidence

- RED-first existing validator did not accept the four new operations; Python case failed with invalid fields, then GREEN with exact wire translations and strict type/range checks.
- RED-first native selectable-next symbol missing; C linker failed, then GREEN after implementation, including zero, unsupported, backwards/wrap, invalid direction and no candidate cases.
- RED-first Rig UI inaccessible if current drawable had unsupported vertex count; new static regression failed, then GREEN with visible previous/next recovery controls.
- MSVC Release avatar_studio + rig dispatch test build PASS, with pre-existing compilation warnings.
- Full Python tests/moc3 plus model-publication guard: 302 PASS, 19 SKIP and 218 subtests PASS.
- Native selected G5/Rig CTest excluding heavy multiprocess fixtures: 15/15 PASS.
- Physical local Native Studio: rig/layer shared selector, valid switch and RigBank restoration, generation and CAS stale rejection, layer aggregate readback and inspector tabs PASS.
- Previously established Rig history, undo/redo, physical Rig → XPBD accepted frames E2E also PASS in separate Studio sessions.
- G4 Native readback: 37 cases, 2 sessions PASS, no private model data published.

## Not closed and next product work

- GUI clicking/keyboard use and visual placement on an actual isolated Studio window were not independently inspected. Controls were compiled and static checks passed; manual usability still UNVERIFIED.
- RigBank supports a bounded maximum of 32 saved edited drawable rigs; history is bounded to 16 selected-drawable snapshots and not restart durable.
- The current Layers tab is the Part opacity inspector plus read-only layer selection. No creation/deletion/reorder, vertex/triangle/UV, skinned or model edits were enabled.
- Layer and Model direct writes require editable source representation, reversible local save/backup, topology validation and independent review. See LAYER_EDITOR_PRODUCT_DESIGN_20261025.md.
- Product G5Bus writer remains HOLD, no G5 APPLIED, no external Agent registry, no new SDK ABI and no publication of model information.

Verdict: Rig selector Native/Agent INTEGRATION VERIFIED, independent GUI QA UNVERIFIED; read-only Layer transition VERIFIED, full Layer Editor NOT IMPLEMENTED; larger Studio goal IN PROGRESS.


## RED-first cross-tab consistency finding

Native Studio E2E observed that selecting a physical mesh in the XPBD tab and
switching to Layers without visiting Rig left the RigBank selected drawable
unsynchronized. Agent rig.selection returned native ERR. This did not prove
any model corruption, but it violated UI/Agent selection parity.

Fix: on entering the Layers inspector, if the currently chosen drawable is
Rig-capable and differs from the active RigBank entry, invoke the same bounded
rig_select path. A successful select updates RigBank, revision and the
selected-layer readback; unsupported drawables remain navigable by previous/next.
The physical test re-ran on a rebuilt Native Studio and switched RED -> GREEN.

Postfix broad regression: 302 Python PASS, 19 SKIP, 218 subtests PASS;
15 selected Native CTest PASS. No new writer or public endpoint.
