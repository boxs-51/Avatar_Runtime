# Studio Local Agent Toolkit - API audit and development direction

Date: 2026-10-25. Branch: exp/g5-local-cas-transactions, local and uncommitted.
Owner goal: useful LOCAL tools to let an Agent operate Studio, not an Agent registry
or a separate G5 infrastructure product. Private model information stays local.

## Real Studio API capability matrix

| Group | Available real API | Missing / limited |
| --- | --- | --- |
| Studio | scene status, metrics, playback, viewport, parameters, parts, camera | No authoring canvas for new layer/model |
| Rig | drawable/vertex select, anchor, beam, role, parent, weight, brush, cross-link, save/load, inspection/G4 readback | Limited hierarchy/skin editor; no multi-command draft in product |
| Rig history | NEW history, undo, redo from both Studio UI and Agent V1 | Bounded 16 edits per selected drawable; reset on switch/restart |
| XPBD | read/set/apply/disable; Rig bind; legacy accepted-frame witness | Manual rebind, physical G5 APPLIED unverified |
| Layer | inspect components/topology, scan, suggest/adopt, semantic label | Cannot create/delete/reorder/reshape layer or edit topology/UV |
| Model | inspect local mesh/rig data with G4/V2, load locally in UI | No direct geometry/material/model authoring or save transaction |
| Safety | local read/write validation; generation/revision for new undo/redo; V1 recovery | Not crash-durable G5 exact-once or a full OS sandbox |

Fixture-only G5DraftTools does NOT count as a product Studio editing capability.
Readback of mesh topology does NOT imply mesh editing is implemented.

## Changes in this checkpoint

- RED-first repro: original dispatcher rejected RigBank selection because a
  generic edit cannot change drawable identity. Dedicated atomic Rig/RigBank
  selection fixes it while ordinary edit identity fences remain unchanged.
- Real owner-thread Rig history: snapshot each accepted edit; undo/redo advance
  revision, fail closed on stale generation/revision, invalidate redo on new
  edits, reset across model or drawable switches; full stack drops oldest safely.
- Validated Agent V1: rig.history, rig.undo, rig.redo; UI Rig Editor has Undo/Redo.
- Isolated real Studio E2E: edit -> undo -> redo, stale denial, bank swap/return,
  no history across drawable, private local model and temp cleanup: PASS.

## Next Studio work, only after Rig is stable

1. **Complete Rig Editor:** UI/Agent parity, hierarchy, skinning/weights, cross
   relations, safe load/save and undo, quantitative XPBD/GPU render acceptance.
2. **Design and implement Layer Editor:** selection, create/delete/reorder/group,
   vertex/triangle editing, UV/topology, constraints and safe previews.
3. **Design and implement Model Editor:** structure, geometry, material, texture
   and parameter editing, reversible validation/commit/save inside local Studio.
4. **Regression:** native UI and Agent E2E, backups, crash handling, performance,
   privacy scans and documented acceptance.

Keep phase/roadmap names and workflow descriptions in docs, not runtime source.
Do not expose models, textures, geometry, paths or other private identifiers.
G5 writer remains HOLD; local history is not a completed G5 multi-command
transaction or a verified G5 document-to-GPU frame guarantee.


## Verified local evidence

- Native Studio MSVC Release build: PASS (pre-existing compiler warnings remain).
- RED-first: missing RigBank switch API produced linker failure; fixed, Native dispatch test PASS.
- RED-first: unknown rig.history/undo/redo failed Python validation, missing C history source blocked CMake; both GREEN after implementation.
- Native CTest for Rig/G5 owner and history: 15/15 PASS, excluding three heavyweight fixture tests.
- Full Python tests/moc3 and publication guard: 297 PASS, 19 SKIP, 208 subtests PASS.
- Real Native Studio isolated E2E: edit, undo, redo, stale revision reject, bank switch/return PASS.
- Physical G4 readback: 37 cases in two Studio sessions PASS.
- Physical V1 recovery: two Studio sessions PASS; recovered original receipt, duplicate denied, restart UNKNOWN, writer HOLD.
- No production G5Bus writer, no public SDK ABI change, no commit/push/merge.

Receipt recovery of operations after its bounded volatile history expires is not guaranteed;
UNKNOWN never authorizes replay. Local edit undo history is not crash-durable.


## Native Rig → XPBD physical workflow follow-up

Agent local Rig anchor/beam/parent/weight edits, revision readback,
undo/redo and four separate GPU-accepted rebind frames PASS in isolated
product Studio. RED-first found stale accepted-frame state after XPBD disable.
Native `xpbd.apply` and `xpbd.disable` now invalidate the old Rig frame.
See [physical frame regression](STUDIO_RIG_AGENT_XPBD_FRAME_E2E_20261025.md).
This is a legacy-frame witness, NOT G5 transaction APPLIED.
