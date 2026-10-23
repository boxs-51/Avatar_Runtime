# ADR-001 — Hierarchical skeletal skinning (proposal)

- **Version:** 0.1 / 2026-10-23
- **Status:** PROPOSED; design-only, no source or public ABI change authorized
- **Owner:** Repository owner, review pending
- **Sources:** SYS-001; Beta QSA audit; `studio_rig.h`, `studio_xpbd.cpp` and `native_mesh_xpbd.hpp`
- **Purpose:** Genuine parent-child transforms and multi-influence skinning. Existing XPBD parent links are **distance constraints, not bones**.

## Alternatives

| Option | Assessment |
|---|---|
| A — retain XPBD distance links | Safe compatibility baseline, no bone transforms |
| B — opt-in 2D bone graph and skinning evaluator | **Proposed**, testable, preserves ABI20 and V4 |
| C — immediately modify public SDK ABI20 | Not authorized; conflicts with release gates |

## Proposed model and frame pipeline

```text
RigDocument V5 (optional)
  bones: ID, optional parent, local bind transform, local pose
  influences: vertex -> up to 4 (bone ID, normalized weight)
  session generation, immutable model/topology identity
  legacy V4 constraints and typed migration metadata

source animation frame
  -> evaluate bone DAG, world = parent_world * local_pose
  -> skin position = SUM(weight * world * inverse_bind * bind_vertex)
  -> XPBD solve relative to skinned driving pose
  -> finite/generation check -> atomic frame accept -> render
```

Mesh/texture source remains immutable. `SkeletonPose` owns transient bone transforms; `SkinningEvaluator` owns proposal vertices; XPBD owns accepted physics state. No two subsystems mutate source coordinates in place.

## Required invariants

- Parent exists; bone DAG acyclic; stable unique bone/vertex IDs.
- Matrices finite and invertible where required; deterministic multiplication/pivot conventions.
- Per-vertex bone weights finite within [0,1], no duplicates, sum normalized within agreed tolerance; maximum four per vertex.
- Unweighted vertices have explicit source/identity behavior, not silent vertex deletion.
- One generation-bound snapshot for pose, skinning, physics and render; stale frames never commit.
- All editor changes are staged and reversible; save only after validation and durable replacement.

## Proposed internal API (not implemented)

`bone.create` / `bone.parent` / `bone.pose` / `bone.inspect`; `skin.weight.set` / `skin.weight.brush` / `skin.inspect`. These must **not** reuse legacy `rig.parent` semantics. No new native write opcode enters service while AUD-F01 remains open.

## Migration and gates

Maintain V1–V4 read compatibility. Opt-in V5 reader rejects invalid records atomically; feature flag defaults OFF, rollback restores V4 XPBD. Work order: close native grammar P1 -> establish IPC ownership and private evidence boundaries -> RED-first synthetic schema -> transform/LBS tests -> XPBD/render integration -> private local E2E -> measured performance -> independent review. No private asset or derived output is distributable.

**Open design choices:** local bone pivot convention, cross-drawable coordinate frames, fixed numerical tolerance, active bone limit and multi-bone brush UX. **Verdict:** PROPOSED / HOLD, not an implementation or release PASS.