# G5 Native ingress convergence — 2026-10-25

Branch: `exp/g5-local-cas-transactions`, local uncommitted.
Privacy: no model assets, geometry, private paths or identifiers published.

## Implementation

- `studio_rig_authority.c/.h`: private Native owner-thread facade mapping trusted UI/V1/Agent ingress to a common `G5Bus` and `G5Txn`.
- Enforces session, generation, revision CAS, request-id uniqueness across ingress, and lease exclusion before creating a candidate. Runs the semantic Rig dispatcher on a copy, then G5 BEGIN, trusted native stage, VALIDATE, COMMIT. Failures preserve live Rig. No-op keeps revision/receipt unchanged.
- UNKNOWN after potential effect is never blindly replayed. Native owner receipts remain recoverable. COMMITTED does not imply GPU APPLIED.
- RED-first: CMake configure initially failed on missing controller; the subsequent model-free MSVC Release implementation passes `g5_rig_shared_authority` covering UI → Agent → V1, stale CAS/model, duplicate ID, foreign lease, no-op, changed model identity, receipt recovery, frame HOLD.

## Release blockers — HOLD

Real Studio UI and V1/Agent still use the legacy owner-thread Rig dispatcher. New G5 authority is linked only to model-free Native tests, **not** the shipping `avatar_studio` target.

Required before product writer cutover:
1. Initialize/rebase across empty Rig, new model and RigBank drawable selection without weakening model identity and session guards.
2. Durable, capacity-safe receipts/undo; the existing G5 receipts/history are capped at 32/16, respectively. A normal editing session must not silently run out of capacity or lose crash recovery.
3. Exact request/owner provenance across local V1/Agent transports, lost ACK, ABA, shutdown and restart.
4. Accepted real document revision → XPBD solver → GPU frame before APPLIED.
5. Full physical UI/V1/Agent concurrency E2E, security and independent review.

No product G5 writes, no fake APPLIED, no commit/push/merge.

## Source policy

Main source retains functional API names, protocol identifiers, invariants and security checks. Phase/stage namespaces, roadmap, migration plan and delivery descriptions live in docs/ADRs, not in main runtime comments. Never rename ABI or serial protocol fields for cosmetic cleanup.


## Verification (Windows Release)

- RED-first: CMake configure initially failed because authority source was absent.
- New owner authority model-free C11 MSVC Release build and CTest: PASS.
- Native opt-in G5 CTest excluding the three process-heavy fixtures: **14/14 PASS**.
- Full Python `tests/moc3` and publication guard: **297 PASS, 19 SKIP,
  204 subtests PASS**.
- Full live Studio/XPBD/GPU, multi-process recovery, and long-session
  capacity/restart are not newly verified; writer stays HOLD.
