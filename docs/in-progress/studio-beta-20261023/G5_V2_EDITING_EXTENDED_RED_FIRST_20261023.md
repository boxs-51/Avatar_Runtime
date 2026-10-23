# G5 V2 Editing — extended RED-first and synthetic oracle evidence (2026-10-23)

## Frozen identity
- Repository: boxs-51/Avatar_Runtime
- Workspace: Windows beta, branch exp/layer-physics-native-studio-local
- Committed HEAD: 2d2fccfaeece6ec686ca09e2e44ae4230574100c
- Existing G4 readback patch: uncommitted; unmodified by this G5 task
- Docs: ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md and ADR_003_RIG_EDITOR_V5_DETAILED_DESIGN.md are PROPOSED, not implementation authority
- Public SDK ABI20: MUST remain unchanged; no commit, push, merge, deployment or Native editor writer activation

## Requirement trace
- G5-REQ-01: Strict session, model generation, edit revision CAS for all editor actions
- G5-REQ-02: Single writer lease, private draft staging, validation, abort without live effects
- G5-REQ-03: Commit only after validation; COMMITTED document receipt is not APPLIED XPBD/frame
- G5-REQ-04: Undo/redo monotonic revisions, divergent branch reject and ABA detection
- G5-REQ-05: Lost ACK after effect means UNKNOWN_OUTCOME, readback/reconcile, no blind replay
- G5-REQ-06: Only frame-accepted physics.apply can report APPLIED with accepted_frame_generation and solver_revision
- G5-REQ-07: Deny untrusted file/path fields, malformed JSON, unauthorized Native mutation; no private data leak
- G5-REQ-08: Existing G4/F02 oracles remain intact; no new Native writer before independent G4 gate

## New test artifacts
- tests/moc3/g5_redfirst_contract.py: existing baseline 4 RED, 2 PASS controls
- tests/moc3/g5_redfirst_lifecycle.py: new extended lifecycle tests, 20 RED and 4 PASS controls
- tests/moc3/g5_reference_model.py: 11 PASS **synthetic reference-model** tests
- All 3 filenames omit test_ prefix: opt-in tests only; default test collection unaffected

## Scenario matrix (RED contracts, NOT production faults)
| IDs | Surface | Intended oracle | Current observation |
|---|---|---|---|
| G5-R01/R05 | draft.begin | Private draft, base revision, no live/solver effect | RED admission unsupported |
| G5-R02/R15/R16 | CAS / single writer | Revision conflict or BUSY, never second lease | RED admission unsupported |
| G5-R06/R07/R08 | draft.validate | Validated non-applied, unknown draft/invalid constraint denied | RED admission unsupported or initial begin fails |
| G5-R03/R09/R10/R11 | draft.commit / abort | No commit of unvalidated/aborted, valid commit distinct from apply | RED admission unsupported or initial begin fails |
| G5-R04/R12/R13/R14 | undo / redo | Monotonic revision, redo divergence denied, no time travel | RED admission unsupported or initial begin fails |
| G5-R17/R18 | stale generation / session | STALE_MODEL / STALE_SESSION without edits | RED admission unsupported |
| G5-R19/R20 | unknown ACK / readback | TIMEOUT_UNKNOWN/UNKNOWN_OUTCOME; reconcile without replay | RED admission unsupported or initial begin fails |
| G5-R21/R22 | document vs solver | COMMITTED != APPLIED; frame acceptance required | RED admission unsupported or initial begin fails |
| G5-C01..C06 | guard controls | Capabilities deny writes, strict JSON, no Native mutations | 6 PASS |

Case group IDs describe the requirement families rather than numeric pytest ordering; use exact test function names for durable per-case identity. Positive and negative future sequence tests now create their draft/validation history before exercising commit, abort, undo/redo or lost ACK. The provisional Native test double has no write effects and cannot prove G5 implementation.

## Actual commands/results (Windows local)
- python -m pytest -q tests/moc3/g5_redfirst_contract.py tests/moc3/g5_redfirst_lifecycle.py --tb=no
  => **24 intentional RED (FAIL), 6 controls PASS**, exit 1.
  This is a **G5 NOT IMPLEMENTED** baseline, predominantly a shared fail-closed admission gate:
  existing V2 rejects non-null session/edit revision and mode != read. These are NOT
  24 independent Native behavioral reproductions.
- python -m pytest -q tests/moc3/g5_reference_model.py --tb=short
  => **11 PASS**, proof tier = synthetic-only reference semantics; NOT connected to production.
- python -m pytest -q tests/moc3/test_studio_g4_mesh_rig_read.py tests/moc3/test_studio_g4_rig_revision.py tests/moc3/test_studio_g4_duplicate_ids.py tests/moc3/test_studio_utf8_ids.py tests/moc3/test_avatar_toolkit_v2_local_ids.py tests/moc3/test_avatar_toolkit_v2_local_read.py tests/moc3/test_avatar_toolkit_v2_epoch.py tests/moc3/test_studio_agent_api.py tests/moc3/test_studio_native_reply_harness.py tests/moc3/test_studio_unknown_outcome.py --tb=short
  => **57 PASS, 85 subtests PASS**.
- python -m pytest --collect-only -q tests/moc3 => 203 tests collected (no G5 opt-in test filenames).
- Python py_compile of RED suite PASS.

## Critical semantic oracles, not yet physically exercised
1. draft.begin -> validate -> commit: new document revision; receipt COMMITTED, not APPLIED.
2. UI/Agent writer lease: one owner only, explicit BUSY or REVISION_CONFLICT.
3. abort/invalid validation: no change to document, physics or accepted frame.
4. undo/redo: generate fresh revisions; diverging commit invalidates redo; ABA restores content but never old revision.
5. post-effect ACK-loss: UNKNOWN_OUTCOME, same request-ID readback; avoid blind non-idempotent retry.
6. physics.apply: APPLIED requires accepted frame and solver revision, not merely a queued/committed document.
7. crash/restart, host GUI input, durable commit, privacy negatives require separate Native physical probes.

## Decision / limits
- G5 T1 tests-only expansion: CODE_PRESENT / RED_BASELINE_REPRODUCED / SYNTHETIC_ORACLE_GREEN
- G5 T2 Native mutation enablement: HOLD. Independent G4 review NOT_RUN and GUI input was previously BLOCKED by Host.
- G5 schema names, state enums, draft.status reconciliation and exact apply receipt are **provisional** pending explicit approval; do not force production code to match an unreviewed mock.
- No security oracle weakened, no model data or native binary writer invoked, no ABI20 changed and no merge.
