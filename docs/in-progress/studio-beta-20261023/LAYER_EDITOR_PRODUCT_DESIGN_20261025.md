# Layer Editor — bounded product design

**Goal:** AVATAR-STUDIO-LOCAL-001 / REQ-003. Local-only Avatar Studio, not a public SDK extension.

## Evidence-based baseline

The existing Layers inspector was built as a Part opacity list; it was not a mesh/layer geometry editor. It now has a read-only selected drawable strip with previous/next supported-layer navigation. Agent V1 has generation/revision-fenced layer.select, layer.inspect and Rig selection readback. These all reference the same live RigBank selected drawable. A physically tested Native Studio session confirms switching and readback. This is not layer geometry editing.

## Interaction layout proposal

- Left: drawable/layer hierarchy, status and selection. Distinguish drawable from Part (Part ID/opacity) to prevent accidental asset editing.
- Center: scene preview and selected layer/vertex/triangle overlays. Begin with read-only highlight, isolate/show controls, coordinate and topology inspection.
- Right: layer properties, geometry statistics and validated edit tools. Rig anchor relationships remain linked to the selected drawable, never duplicated.
- Bottom: staged edit summary, preview/validate, commit or abort, undo/redo and local save/backup. Status MUST distinguish draft, committed and XPBD-rendered.
- Agent: calls use the same selected drawable and one Studio-owned candidate/commit path as UI. No separate Agent model state, API access-manager or network service.

## Expected interaction states

1. NO_MODEL: read/select disabled, no stale layer ID reused.
2. SELECTED_READ_ONLY: display bounded metadata and topology status, switch next/prev; inaccessible/oversized drawables are skipped by the Rig picker.
3. DRAFT_EDIT: only after a local data-ownership and transaction contract is implemented. No immediate destructive change to source assets.
4. VALIDATE/PREVIEW: topology integrity, vertex and texture indices, Rig/XPBD references, stable model generation and revision CAS, undo snapshot.
5. COMMIT_LOCAL: atomic user-confirmed live layer replacement, UI/Agent coherence, backup and reversible history.
6. XPBD_FRAME_ACCEPTED: only after solver and GPU accepted the bound layer/Rig revision. If disabled, swapped or stale, mark frame not current.
7. ABORT/ERROR: preserve previous live state, report bounded error, never export private data.

## Future functional order (not implemented here)

1. Read-only selected layer overlay and vertex/triangle inspector; pagination for large models.
2. Layer hierarchy and grouping; visibility, reorder and rename as validated reversible local edits.
3. Vertex manipulation, triangle topology and UV editing with protected model format validation.
4. New/delete/duplicate layers and attachment relationships, cross-layer skinning and physics rebind.
5. Save/reopen against private local asset formats with backup/recovery, quantitative visual and performance tests.

## Exit gates before direct layer/model writes

- Rig selection and bank history demonstrate real UI mouse/keyboard as well as Agent E2E.
- The representation to be edited (MOC3 immutable runtime vs local editable source) is identified and validated. Do not write directly into opaque compiled MOC3 buffers by guessing.
- Undo/redo, crash-safe local backup, topology and bounds validations are specified and tested RED-first.
- Private-model publication guard and SDK ABI20 regressions remain green.
- Separate independent review of direct layer write paths. No G5 writer activation inferred from the design.

**Status:** Layer selection/inspection implemented and physically verified for Agent. Direct Layer Editor geometry writes DESIGN ONLY; GUI interaction usability and model persistence not yet verified.
