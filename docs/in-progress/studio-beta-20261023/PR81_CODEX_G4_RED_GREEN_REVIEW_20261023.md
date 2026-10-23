# PR #81 Codex Review Remediation — G4 Local-Only

**Repository:** `boxs-51/Avatar_Runtime`
**Branch:** `exp/layer-physics-native-studio-local`
**Codex-reviewed baseline:** `7fdd7e2fa06a4a2928cc0332d948544b48bda7e9`
**Initial review:** [PR #81](https://github.com/boxs-51/Avatar_Runtime/pull/81), 2026-10-23; P1 x1, P2 x2. No GitHub Actions waiver is treated as review approval.

## Finding P1 — false-zero staged UI verdict

Location: `scripts/test_studio_g4_ui_race_live.py::finish`. Before fix, terminal verdict `SEMANTIC_RED_MIXED_SNAPSHOT` returned 0. RED-first test reproduced false PASS (test_studio_g4_codex81_redfirst.py). Fix: exit 0 only for `REJECTED_AFTER_EDITOR_CHANGE` and `REJECTED_ABA_STALE_REVISION`; harness/environment BLOCKED returns 3; any other failed/unexpected verdict returns 2. No assertion disabled.

## Finding P2 — phase-end ABA when two UI edits cancel

Location: `tools/avatar-studio/studio.c`. Before fix, one phase-end `memcmp(before, after)` could miss two accepted inverse mutations. Fix: owner-thread per-edit UI checkpoint `studio_note_ui_rig_edit()`, called after each direct UI/viewport mutator (anchor, beam, parent, weight, brush, cross, load, bank switch, proposal adoption) and at phase end for any remaining untracked net state. Each call advances `rig_revision` when its immediate edit changed the state, even when another later UI edit restores the original bytes. A source-wiring regression asserts instrumented sites. A standalone click-driven same-frame GUI physical oracle remains **NOT_RUN**, not claimed as proven.

G4 `rig_revision` is still a bounded-read observation fence; it is **not** G5 authoritative `document_revision` or a full transaction journal. The G5 Native writer remains HOLD.

## Finding P2 — malformed Rig relation counts exceed vertex capacity

Location: `scripts/avatar_toolkit_v2_local.py::_g4_metadata`.
Additional mesh-relative caps for G4 rig metadata: anchors <= vertices (unique vertex), weights <= vertices (one per vertex), parents <= vertices - 1 (acyclic), beams <= vertices*(vertices-1)/2 (unique unordered pairs); existing fixed capacities 64/256/64/128 and cross cap 16 preserved. RED-first malformed receipt with vertices=3 and anchors=4 was previously `COMPLETED_READ`, now `REJECTED / INVALID_DRAFT`. Exact valid three-vertex bounds remain accepted.

## Validation and proof tiers

- RED-before-fix: 2 failing focused test functions (P1 verdict mapping, P2 mesh-count validation), valid bounds control PASS.
- Post-fix: 4 focused tests PASS, including static UI call-site coverage. The static source wiring check is **NOT** a physical same-frame UI E2E.
- Full Python `tests/moc3`: 207 PASS, 18 SKIP, 204 subtests PASS.
- Windows Release Studio build: PASS; existing MSVC warning baseline not suppressed.
- Native G4 readback: 37/37 PASS, 2 sessions; cross 9/9 PASS; Agent/Agent ABA `REJECTED_ABA_STALE_REVISION` PASS.
- CTest Windows Release: 10/10 PASS including public SDK ABI20 C11/C++20.
- No private model bytes/captures intended for Git, test reports retain outcome-only receipts.
- GitHub Actions status quota / RC1 base drift: explicitly waived as a workflow requirement by Owner in current request; **not** evidence of a green build or an authorization to weaken RC1.
- Independent Codex re-review at post-fix HEAD is REQUIRED before claiming reviewer finding closure.
- Main merge must additionally consider beta's entire 36+ commit integration scope and the one-commit base drift; Codex's scoped G4 review alone does not prove unrelated functionality.

## Delivery

Keep G5 transaction CAS/recovery/undo/redo design PROPOSED and Native writer disabled. No model export. No automatic merge until independent review is complete with no open P0-P2, and merge scope/security checked.
