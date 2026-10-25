# P1 Rig–Layer Graph | 2026-10-25

Scope: approved P0–P4A on local Avatar_Runtime beta.
P4B, independent UX and P5 HOLD. No commit/push/merge/deploy.

## Implemented

- Layer and vertex stable IDs; stale epoch/revision/ABA guards.
- Directed cross-layer Rig links by layer ID + vertex ID, weight/stiffness,
  cycle and duplicate rejection, deletion guards and undo/redo snapshots.
- STUDIO_GEOMETRY_5 source with backward readers V1–V4.
- ARSMESH3 local compiled runtime with backward ARSMESH1–2 read support.
- Multi-layer native XPBD supports 2–8 authored layers and up to 16
  directed parent relations with per-layer anchor/weight constraints.
- GPU fence checks bound revision, per-layer physical point readback.
- Shared Native Links tab: mark parent, choose child vertex, link,
  inspect paginated links, unlink. Mark invalidated after data revision.
- Cubism MOC3 remains read-only; G5 writer HOLD.

## Current verification

Native isolated Studio E2E: two and three custom layers, 5+ accepted
GPU frames, physical movement, two pinned anchors, stable IDs,
source/binary roundtrip, stale reject, cycle/duplicate/referenced-delete
denial. Restart in independent Native Studio session reloads source
and ARSMESH3 and re-accepts three-layer GPU frames. Private MOC3 unchanged.
Release build PASS; C unit, Python contract, previous local workflows PASS.
Final regression on same worktree:
- Python pytest tests/moc3 + publication guard: 310 PASS, 19 SKIP,
  229 subtests PASS.
- Release Native CTest selected: 17/17 PASS.
- Real local Native E2E: two-layer and three-layer graph and restart,
  single-layer GPU frame, geometry create/save/reopen, import, restart
  corruption denial, and independent legacy Rig physics: all PASS.
- Earlier RED tests for ID lookup, vertex identity and cyclic edge
  are now GREEN.
- GUI interaction on actual window: NOT RUN, remains HOLD.

## Unclosed acceptance gates

P1-R1: actual GUI click/keyboard test on verified isolated HWND blocked
by Host permission gate (DEC-07=A); UI build alone is NOT GUI PASS.
P1-R2: independent Codex review for exact worktree diff NOT RUN;
a coder self-review cannot satisfy this gate.
P1-R3: more physical stress, cross-layer collision and broader
per-drawable skinning/multiple anchors remain unverified.
P1-R4: crash-durable source backup/recovery remains unverified.
P1-R5: original compiled Cubism MOC3 edit/export intentionally excluded.

Verdict: P1 implementation substantially advanced, final acceptance
UNVERIFIED/HOLD. Roadmap requires P1 gate before beginning P2.
This file is not proof of production/release readiness.
