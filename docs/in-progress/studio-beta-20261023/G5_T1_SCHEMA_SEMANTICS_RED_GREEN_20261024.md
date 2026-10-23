# G5 T1 CAS Schema Semantic Hardening — 2026-10-24

**Repository:** boxs-51/Avatar_Runtime
**Workspace alias:** beta (Windows)
**New branch:** exp/g5-local-cas-transactions
**Base:** main@807ac53ec352e8b4cd1b821a61364e810569e202, merged PR #81
**Scope:** G5 Local-only proposed contract/schema and opt-in tests; not an approved executable API. No G5 Native writer, public ABI20, model exports, or merge into main.

## Baseline gate inherited from G4

PR #81 was merged after Codex code re-review at 7420c88585532823747fb5bf5d720e88610d043a reported completed / zero new findings. Local main was fast-forwarded to the GitHub merge commit and new G5 branch created and pushed. GitHub Actions were temporarily waived by Owner; their prior RC1 base-drift failures were not represented as successes. UI click-based same-frame physical proof and G5 transaction semantics remain separate gates.

## Requirement-first T1 findings

Proposed schema: docs/in-progress/studio-beta-20261023/G5_LOCAL_CAS_ENVELOPE_PROPOSED_20261023.schema.json

- G5-SCHEMA-01: A COMMITTED response had been allowed to set accepted_frame_generation to a positive number despite applied=false. Contract requires accepted_frame_generation be null if present; COMMITTED is not APPLIED.
- G5-SCHEMA-02: UNKNOWN_OUTCOME had allowed a definitive COMMIT_FAILED error. Require TIMEOUT_UNKNOWN for the transport-ambiguous top-level response. This is not a claim that the owner did not commit.
- G5-SCHEMA-03: COMPLETED_READ responses exposing recovery outcome had accepted arbitrary strings, including false "CERTAIN_NOT_APPLIED". Constrain outcome to PENDING, COMMITTED, REJECTED, ABORTED, UNKNOWN. Require origin request ID and bounded document/applied/frame receipt fields. An absent receipt maps to UNKNOWN, never automatic replay authorization.

All changes are **PROPOSED schema semantics**; the runtime V2 decoder remains read-only and rejects mutation modes. Recovery transport isolation (dedicated read-only lane), owner receipt index, durable crash semantics, and UI/Agent shared Native authority are not implemented.

## RED → GREEN proof

Test file: tests/moc3/g5_schema_redfirst.py (no test_ prefix; opt-in, excluded from default pytest collection).

- Before schema changes: 3 failed, 8 passed (semantic failures, not imports/environment).
- After schema changes: **11 passed**.
- No weakening of the original opt-in T1 G5 tests or their expected RED.
- G5 reference model existing suite: 11 PASS, synthetic-only.
- G5 V2 integration RED baseline: 24 intentional RED, 6 control PASS (runtime Native writer remains disabled).
- Full default tests/moc3: 208 PASS, 18 SKIP, 204 subtests PASS. None of these 18 SKIPs is declared PASS.
- git diff --check: PASS. No ABI20 paths or shipping Native editor code changed.

## Remaining contract ambiguity

The older proposed G5 integration oracle lists NOT_APPLIED as a possible draft.status outcome, while this stricter proposed schema uses UNKNOWN, PENDING, COMMITTED, REJECTED and ABORTED as the only recovery outcomes. NOT_APPLIED is not an unambiguous transaction result; adjust the test **only after the exact contract is independently reviewed**, preserving fail-closed semantics.

JSON Schema alone cannot prove request-ID ownership, model generation authority, receipt linearization, crash persistence, private data safety or frame acceptance. Those need Native physical RED-first T2 evidence.

## Gate

G5 T1 schema tightening: GREEN for the opt-in syntax/semantic checks.
G5 T2 Native mutation: HOLD.
Independent G5 transaction contract review: NOT_RUN.
G5 live lost-ACK, UI/Agent simultaneous edits, undo/redo lineage and COMMITTED-vs-APPLIED physical proof: NOT_RUN.
