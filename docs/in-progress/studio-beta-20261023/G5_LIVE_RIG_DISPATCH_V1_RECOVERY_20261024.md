# G5 Stage — Live Rig Dispatcher and V1 Recovery (2026-10-24)

**Branch:** exp/g5-local-cas-transactions, local, uncommitted.
**Base HEAD:** 0f8602fef213310af64a37815068cdfd44a38fe2.
**Scope:** UI/V1 Agent Rig edits and real Studio local receipt recovery.
**Exclusions:** G5/V2 writer enablement, public API, Agent registry, model publication.

## Shared Rig command dispatcher

- Private/reusable C11 `studio_rig_dispatch.c/.h` copies the current Rig,
  validates generation and expected legacy revision, applies a command on
  a candidate, then commits atomically and increments revision once.
- UI and V1 Agent Rig actions route through the same owner-thread dispatcher:
  anchor, beam, parent, weight, cross-link, brush, role, rig load, proposal
  adoption and drawable/RigBank selection. RigBank is staged and committed
  only after an accepted candidate.
- Model destruction resets local state as lifecycle cleanup. XPBD `rig.apply`
  is a solver action, not evidence of G5 document/frame acceptance.
- Compatibility revision checks avoid double-increment for dispatched edits.
- Tests: synthetic C11 stale CAS, rollback, no-op, revision increments;
  static no-direct-bypass source oracle; physical Studio G4 ABA regression.

This is the **shared legacy Rig edit dispatcher**, not yet an enabled
`G5Bus` document writer. G5 draft/undo/redo across the real UI/V1/Agent
and XPBD/GPU `APPLIED` acceptance are separate remaining HOLD gates.

## Product V1 read-only recovery IPC

- New C11 `studio_v1_recovery.c/.h` stores at most 64 original request
  tokens and original OK/ERR outcomes **in memory**. No raw model payload,
  textures, geometry, private paths or response content are retained.
- Native Studio polls `api/recovery/request.txt` separately from the
  mutation queue. Input: `<new-token> RECOVER <original-token>`.
  Output: `<new-token> FOUND OK`, `FOUND ERR` or `UNKNOWN`.
- Known duplicate V1 mutation tokens are denied without reapplying.
  After model reload or Studio restart, lost volatile receipts are UNKNOWN,
  which is **not** proof of no effect and does not authorize replay.
- Python `AgentBridge.call` raises `StudioUnknownOutcome` (a TimeoutError)
  with the original token on lost ACK. `recover` reads the separate lane.
  `reconcile` under exclusive mutation writer lock clears the client intent
  **only** after a matching real Studio FOUND receipt. Agent task helpers
  `recover_v1` / `reconcile_v1` expose these functions without Agent accounts.
- Physical Windows E2E `scripts.test_studio_v1_product_recovery_live`
  verifies two Studio sessions, receipt lookup, duplicate denial,
  post-effect ACK-loss quarantine, wrong-token rejection, explicit
  reconciliation, and UNKNOWN after restart.

## Verification and limits

Synthetic G5 CTests add `g5_rig_owner_dispatch` and
`g5_real_studio_v1_recovery`. Shipping `avatar_studio.exe` Release builds.
Existing physical G4 checks passed after refactoring: 37 native cases,
9 cross-drawable cases and the ABA stale-revision case. The static oracle
`tests/moc3/test_studio_rig_dispatch_route.py` rejects bypass calls.

**HOLD:** Receipt history is volatile and bounded; not crash durable or
exactly-once after restart. Native G5Bus is not yet the common real
UI/V1/V2 document writer. XPBD/GPU accepted-frame is unverified. No
independent reviewer sign-off has been obtained. No OS sandbox/Agent
identity-management guarantee is claimed. Private model information is
not to be published (ADR-005).

**Delivery:** local worktree only, no commit, push, merge or deployment.

## Verified local checkpoint (Windows Release)

- Python regression: **285 PASS, 19 SKIP, 204 subtests PASS**.
- Native opt-in G5 CTest: **14/14 PASS**; top-level ABI20/project
  CTest: **10/10 PASS**; Studio Release build: **PASS**.
- Physical Studio V1 recovery E2E: **PASS, two sessions**, including
  deliberately lost ACK, explicit reconciliation, duplicate denial,
  and UNKNOWN after restart.
- Physical G4 regressions: **37 Native cases PASS**, **9 cross-drawable
  cases PASS**, UI/Agent ABA case **REJECTED_ABA_STALE_REVISION**.
- Static source routing and revised Codex81 UI revision hook oracle: PASS.
- Publication scanner on committed HEAD (662 blobs): PASS; local changes
  scanned for known sensitive formats and secret patterns: PASS.

**Bounded-history caveat:** The real V1 receipt ring retains only the most
recent 64 commands. Duplicate protection and FOUND recovery for older,
evicted tokens are NOT guaranteed; clients must never replay commands
merely because lookup returns UNKNOWN. Reliable post-crash/idempotent
operation tracking across an unbounded session remains a separate G5 gate.
No claims of exact-once, durable recovery or finished G5 document routing.
