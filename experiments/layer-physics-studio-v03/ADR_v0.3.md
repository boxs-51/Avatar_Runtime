# ADR-LPS-003 — Interlayer XPBD experiment and direct native2D preview

Status: PROPOSED / non-production, owner: Avatar_Runtime owner, research baseline alpha `006f36689d1c432659925ea2f9abe865a7825831`.

## Decision

Keep the authoring editor outside the shipped ABI20. Preserve the accepted-state/candidate split: `stepState(doc,accepted)` returns a new state or throws; it never mutates input. Solve all candidate meshes in a global fixed 1/120s step. Local stretch/shear/bend distance constraints use compliant XPBD (per-step lambdas); additional cross-layer distance joints enforce rest lengths and transfer force to both free endpoints weighted by inverse mass. A deterministic particle-circle broadphase resolves overlapping pairs on different enabled layers. Only committed candidates render. Per-layer radius/collision flags and max pair checks bound work.

## Data ownership

`doc` is user-authored and versioned `ars.layer-physics-authoring` v0.3.0; `layers[]` is a typed tree with stable IDs; `joints[]` references `{layerId,index}` endpoints; `world.collisionEnabled` and per-layer flags gate contacts. The app may render RGBA preview textures; no image or file contents are sent to a server. `accepted` contains positions, previous positions and internal constraint data; never treat browser-prepared CPU data as validated runtime model handles.

## Gate limitations

Interlayer collision treats each particle as a circle, does not guarantee continuous collision, supports no cloth self-collision or mesh edges. Joint between a pinned and free endpoint will behave one-way as intended; joints between free particles have bilateral reaction. Parent->child attachment tips remain kinematic convenience, not true bilateral joints. Rest-length changes require configuring another document revision. Movement under authored rig poses and full imported model rendering are not yet integrated. The .arschar importer is dev-only and excludes multi-vertex rig deformation, compositing masks, 3D GLB/MOC3 and alpha D3D11 parity. The complete full-model native acceptance remains HOLD.

## Threat model and regressions

Reject unknown document versions, cycles/duplicate IDs, nonexistent joint endpoints, non-finite values, excessive layer/particle/joint budgets, unauthorized/unexpected image MIME, oversized ZIP, traversal and archive duplicates. Keep prior accepted state after candidate errors. Unit matrix: interaction bilateral vs disabled, collisions vs disabled, deterministic replay, pinned anchors, migration from older version, malformed assets, UI drawing. Production validation remains separated from this proof of concept.

## Rollback

Keep v0.2 ZIP unchanged, create only isolated `exp/...` branch, no alpha/main edits. Authoring JSON v0.3 is separate from original bundle and the original asset is never rewritten. Disable all collisions or delete joints to compare behavior. Any failure rejects candidate without overwriting alpha or trusted model contents.
