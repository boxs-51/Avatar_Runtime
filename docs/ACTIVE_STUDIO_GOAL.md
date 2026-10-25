# ACTIVE GOAL — Avatar Studio Local Agent Toolkit

Goal ID: AVATAR-STUDIO-LOCAL-001 | Contract C0 | 2026-10-25

Owner intent: finish reliable Rig selection in the Studio UI and Agent Local API, then work on Layer Editor, then Model Editor. Keep this priority pinned and do not expand G5 infrastructure.

| ID | Requirement | Proof |
| --- | --- | --- |
| REQ-001 | Pin this priority in the main repo entry points; Studio remains separate from SDK runtime | Document |
| REQ-002 | UI/Agent Rig selection uses one selected drawable; invalid/stale selections have no side effects; valid switch preserves RigBank and scopes history | Native unit + physical E2E |
| REQ-003 | Start the Layer Editor with shared selection and private readback, without asserting direct layer/model editing is ready | Native integration + design |
| REQ-004 | Preserve legacy XPBD/GPU witness, privacy, existing local API, ABI and regression gates | Regression + physical E2E |

Non-goals: new G5Bus features, agent registration, HTTP/cloud APIs, public models, new SDK ABI, automatic deployment/push/merge, prematurely enabling mesh/UV writes.

Observation: the current Layers tab is a part-opacity inspector, NOT a direct layer geometry editor. Read-only layer selection and inspection can bridge into design while the separate authoring transaction/backup/preview contract is prepared.

Milestone 1 - Rig selector: deterministic previous/next valid drawable, epoch/revision-fenced Agent select/readback, same selected layer for UI and Agent, robust bank switch, physical Studio proof.
Milestone 2 - Layer Editor: inspect topology and vertices, design layer hierarchy, select, create/edit/remove/reorder with bounded private draft/validate/undo/save and real render verification before enabling writes.
Milestone 3 - Model Editor: after Layer Editor stability, design local mesh/parameter/material/model edits and backup/recovery.

Resume rule: read this file plus the Studio capability audit before new Studio work. Roadmap and implementation stage labels belong in documents, not product code. G5 real product writer remains HOLD, legacy frame evidence is not G5 APPLIED. No push, merge or publication without owner instruction.

Current completion (2026-10-25 checkpoint):
- REQ-001 VERIFIED: goal pinned in README, docs index and this file.
- REQ-002 INTEGRATION VERIFIED: Native Agent selection, RigBank return, epoch/CAS, unsupported-geometry navigation and cross-tab XPBD→Layers synchronization physically tested. Manual GUI pointer/keyboard interactions remain UNVERIFIED.
- REQ-003 LOCAL GEOMETRY EDITOR: custom sidecar layers now support create/edit/delete, import compiled drawable copy, vertex/triangle edit, duplicate/order, undo/redo, save/load; Native Agent E2E and PID-fenced GUI clicks PASS. MOC3 source-layer authoring, UV/material semantics, accepted XPBD frame for custom geometry, visual QA and independent review remain HOLD.
- REQ-004 REGRESSION VERIFIED for tested scopes: 302 Python PASS, 19 SKIP, 218 subtests PASS; 15 selected CTests PASS, real Rig→XPBD frame tests PASS. Separate independent review NOT DONE, therefore release gate remains HOLD.

Proof: [Rig selector / Layer handoff](in-progress/studio-beta-20261023/RIG_SELECTION_TO_LAYER_HANDOFF_20261025.md) and [Layer Editor product design](in-progress/studio-beta-20261023/LAYER_EDITOR_PRODUCT_DESIGN_20261025.md). Update only with observed proof; do not self-declare Layer or Rig complete.

Active Layer Editor checkpoint: [Native Layer Editor viewport](in-progress/studio-beta-20261023/LAYER_EDITOR_NATIVE_VIEWPORT_20261025.md). Preserve Rig selection, skip new G5 infrastructure.

Geometry checkpoint: [Native local geometry editor](in-progress/studio-beta-20261023/LAYER_EDITOR_LOCAL_GEOMETRY_20261025.md). Next: source-editable Layer/Model representation, UV/material and native physics integration.

## UI/UX review gate

User-added parallel objective (2026-10-25): make the Native Studio easy to operate, without reducing the completeness of the Layer Editor and future Model Editor functionality. A generated concept image has been presented for owner review in ChatGPT; no UI implementation is authorized merely by generating the concept.

- Keep all existing and planned functional tools reachable through coherent Layer / Mesh / UV / Material / Rig / XPBD workspaces. Prefer progressive disclosure, keyboard shortcuts and discoverable actions over deleting or hiding essential features.
- Show selected layer, revision, undo/redo, validation, local save and preview state clearly. Distinguish compiled-model (read-only) layers from editable local sidecar layers.
- Obtain owner approval of conceptual layout before restructuring production UI. Use a separate Studio test window verified by PID/foreground for GUI automation; never send scripted input to personal applications.
- Acceptance requires a feature-to-UI/API coverage matrix, real GUI interaction tests, and readable layouts at supported window sizes; an attractive mockup alone is not proof of working functionality.
- Preserve local model privacy. No push/merge without explicit instruction.


## Current shared UI + Layer UV/Material/Physics checkpoint

- Native Studio left and right sidebars can collapse independently to
  increase the editing viewport. Context inspector includes Geometry,
  UV/Material, and Rig/XPBD sections; bottom history/timeline dock is planned.
- Local layer sidecar V2 now stores per-vertex UV and material tint/opacity,
  with backward V1 load and a source-editable SVG layout reference.
- Local custom mesh uses the native XPBD solver core with a pinned vertex;
  physical displacement, pin stability and GPU-accepted frames have Native
  Studio E2E evidence. This does not imply full RigBank/MOC3 integration.
- Current Host GUI input cannot be certified: SendInput denied and older
  mouse injection failed. Maintain strict PID/foreground/window fences.
- See docs/in-progress/studio-beta-20261023/SHARED_UI_UV_MATERIAL_XPBD_20261025.md
  and docs/ui/AVATAR_STUDIO_SHARED_LAYOUT.svg. Keep user-approved concept
  PNG references distinct from this editable schematic.

## Rig and native compiled mesh checkpoint

- Editable layers support Rig influence weights and acyclic parent links,
  saved with sidecar V3 and loaded into the native custom XPBD solver.
- Studio source compiles to its own validated ARSMESH1 runtime binary
  and reopens in a different Native Studio session. This is NOT .moc3.
- Compiled Cubism MOC3 remains unchanged. A supported .cmo3 Cubism
  Editor source workflow is necessary for real proprietary MOC3 export.
- See docs/in-progress/studio-beta-20261023/LAYER_RIG_NATIVE_MODEL_RUNTIME_20261025.md

## Owner-approved roadmap scope (2026-10-25)

Read [AS-ROADMAP-001 v1.0](in-progress/studio-beta-20261023/AVATAR_STUDIO_ROADMAP_OWNER_REVIEW_20261025.md) before new work. Owner APPROVED FOR IMPLEMENTATION P0, P1, P2, P3, P4A; P4B, UX independent phase and P5 remain HOLD. Choices: DEC-01=A; DEC-02=B; DEC-03=A; DEC-04=A; DEC-05=A; DEC-06=A; DEC-07=A; DEC-08=B. Independent UX remains HOLD, although UI implementation required by an approved phase is included in that phase. P0 must pass before implementing P1 and subsequent gates. Prior test evidence is historical, not release PASS. No commit/push/merge/deploy, public model upload, compiled MOC3 edits or G5 writer unless Owner explicitly authorizes them.

## P0/P1 actual progress checkpoint (2026-10-25)

P0 contract/inventory verified in [P0 baseline](in-progress/studio-beta-20261023/P0_BASELINE_CONTRACT_GATE_20261025.md). P1 stable layer+vertex identities, directed custom cross-layer Rig links, multi-layer XPBD GPU E2E and local ARSMESH3/source V5 restart implemented. [P1 gate report](in-progress/studio-beta-20261023/P1_LAYER_RIG_GRAPH_GATE_20261025.md) remains UNVERIFIED/HOLD for independent review, authorized GUI input and broader physical stress. P2–P4A are Owner-authorized but must respect the preceding phase gate. P4B, independent UX and P5 HOLD. No Git commit/push/merge.
