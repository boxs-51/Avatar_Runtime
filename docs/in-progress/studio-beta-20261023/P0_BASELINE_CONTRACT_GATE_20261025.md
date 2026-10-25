# P0 Baseline and Data Contract — 2026-10-25

Scope: P0–P4A approved on beta. P4B/UX independent/P5 HOLD. No Git push/merge/commit.

## Data ownership

- Native authoring source: STUDIO_GEOMETRY_3 local sidecar.
- Binary runtime: ARSMESH1, model_runtime.armesh; never Cubism MOC3.
- Compiled SDK MOC3: read-only, private. Previous G5 writer: HOLD.

## Feature -> UI -> Agent -> Native -> Tests

| Capability | UI | Local API | Native consumer | Evidence | Scope |
|---|---|---|---|---|---|
| Epoch/status | viewport | studio.status/epoch | Studio owner | live | VERIFIED |
| Create/delete/reorder | Mesh | layer.create/delete/reorder | geometry doc | unit/live | bounded |
| ID resolve/select | layer | project.by_id/active.id | document ID/CAS | RED/GREEN, live | VERIFIED |
| Vertex/triangle | viewport | point/face operations | document | native/live | bounded |
| UV/material | Inspector | uv.set/material.set | sidecar/checker | live | debug material |
| Rig pin/weight/parent | Rig panel | rig.pin/weight/parent | document/XPBD | live | intra-layer |
| Cross-layer Rig | none | none | none | none | P1 HOLD |
| Physics | XPBD panel | layer.physics.* | native_mesh_xpbd | GPU witness | one layer |
| Compile/load | Build panel | model.compile/open | ARSMESH1 | live restart | ARS only |
| GUI controls | Native window | n/a | PID/HWND fence | Host blocked | HOLD |
| MOC3 author | none | none | SDK reader only | none | P4B HOLD |

## Identity contract

Layer ID is unique 1–9999 and survives reorder. Layer index is only
presentation position. by-ID queries/select require epoch and latest
document revision, reject stale and missing/deleted IDs. Reused numbers
after deletion represent new objects; clients must refresh revision.
Existing index-based commands are compatibility-only, and mutations
continue to be revision-fenced. Vertex IDs are still mutable indices.
Cross-layer attachments must not use positions that change on deletion.

## Migration and backup

V1 geometry; V2 UV/RGBA/pin; V3 Rig weight/parent. Load validates
source signature, finite coordinates, triangle topology, parent cycles
and bounds. Invalid load preserves active document state. ARSMESH1
checksum is not a cryptographic authenticity guarantee. Sidecar backup
is best-effort, not proven crash-atomic recovery.

## Findings assigned to later approved gates

- P0-ID-01 / P1: vertex IDs shift after topology edits; define stable
  per-vertex references with migration and ABA tests.
- P0-ID-02 / P1: legacy selection is list-position only; use by-ID for
  persistent Rig references.
- P0-RIG-03 / P1: custom XPBD binds a single layer; no cross-layer link.
- P0-FS-04 / P2/P4A: verified rollback/crash durability incomplete.
- P0-TEX-05 / P2: checker texture does not replace local source atlas.
- P0-GUI-06 / P2/P3: Host denies synthetic GUI input. Do not bypass.
- P0-REV-07 / P1: no independent review of exact final diff yet.
- P0-CUB-08 / P4B HOLD: no MOC3 source compiler authorized.

## Baseline and bounded P0 correction

- Python: 310 PASS, 19 SKIP, 229 subtests PASS.
- Native CTest selected: 17/17 PASS.
- RED-first: missing stable by-ID protocol failed Python validator;
  native test failed linking absent studio_geometry_index_by_id.
- GREEN: bounded API contract and native identity test PASS.
- Release Native Studio build PASS.
- Private isolated Studio Agent E2E PASS for stable ID after reorder,
  stale revision denial and deleted ID rejection. Model unchanged.

P0 DOCUMENT GATE: PASS for ownership, matrix, identity contract,
dependency and assigned findings. This is NOT P1 completion or release
approval. P1 implementation begins with stable vertex identity and
cross-layer links; P1 gate requires two-layer physical E2E and
independent review before proceeding to P2.
