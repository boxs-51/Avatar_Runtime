# G5 ACK Recovery — Exact Original Command Binding

**Date:** 2026-10-24  
**Branch:** `exp/g5-local-cas-transactions`  
**Base before this patch:** `804f824951287f9c9ac89330cda751a45ba5bd86`  
**Scope:** Internal, Native G5-only. No Studio/V2 writer activation, public ABI20 changes, private model files, main merge or deployment.

## Finding G5-ACK-05 (P1 release gate)

At the inspected base, `g5_bus_reconcile_trusted` accepted a matching owner/session and terminal COMMITTED receipt solely by **request ID**, even though the receipt already stored operation and canonical SHA-256 of the submitted command. `g5_bus_mark_unknown_trusted` accepted only a numeric request ID, so a different original command or a confused/mismatched payload could clear an outstanding UNKNOWN_OUTCOME marker. This could permit a future Agent to continue after reconciling the wrong command.

This is an internal pre-release correctness/security gate, not a demonstrated live Studio or IPC exploit: the G5 writer is not connected to the shipping application.

## RED-first acceptance matrix

| Test | Input | Expected owner-thread result |
|---|---|---|
| ACK-05-A | Same actor and request ID, but altered `expected_revision` in the original COMMIT envelope | `G5_ID_CONFLICT`, quarantine retained |
| ACK-05-B | Same actor and request ID, but changed opcode COMMIT→UNDO | `G5_ID_CONFLICT`, quarantine retained |
| ACK-05-C | Exact matching opcode and canonical command bytes | Reconcile succeeds, that actor's quarantine cleared |
| ACK-05-D | No receipt for original request | `G5_UNKNOWN`, quarantine retained; no automatic retry |
| ACK-05-E | Mark uncertainty on BEGIN/read or other nonterminal operation | `G5_INVALID_DRAFT`, no quarantine |
| ACK-05-F | Session, generation or ingress mismatch | `G5_STALE_SESSION` / `G5_STALE_MODEL`; no marker |
| ACK-05-G | Other Agent using same ingress category | Cannot reconcile the first actor's receipt or clear their quarantine |
| ACK-05-H | Full fixed-size quarantine table | Reject before evicting an outstanding unknown |

Tests were authored/updated before the implementation. **Actual Native RED run: NOT_RUN**, because Windows Computer Bridge is offline and GitHub Actions remain quota-limited. The old source-level weakness was verified by inspection; that is not a substitute for execution evidence.

## Changes

- `studio_g5_bus.h`: `G5ActorQuarantine` now retains operation and a 32-byte digest bound to the original request.
- `studio_g5_bus.c`: new `g5_bus_mark_unknown_command_trusted` accepts the complete immutable original command and a trusted Host-bound actor. It checks ingress, session epoch, model generation and mutation opcode; it computes the SHA-256 internally over the canonical v1 command envelope. The caller does **not** supply its own digest.
- `g5_bus_reconcile_trusted`: requires owner-scoped terminal receipt **and** operation and SHA-256 matching the stored marker. Mismatch returns `G5_ID_CONFLICT`, leaving quarantine intact.
- Numeric-only `g5_bus_mark_unknown_trusted` is behind `G5_TEST_ONLY_UNBOUND_BUS` for historical synthetic test compilation only; new integrations must never expose that unbound path.
- `studio_g5_actor_receipt_redfirst.c`: migrated to full-command marker, retains existing actor isolation and bounded quarantine oracles.
- New `studio_g5_receipt_binding_redfirst.c`: command mismatch, opcode mismatch, positive matching receipt, absent receipt, invalid provenance.
- `tools/avatar-studio/CMakeLists.txt`: new test registered **only** with opt-in `STUDIO_G5_ACTOR_RED_FIRST=ON`; no change to `avatar_studio` executable sources.

## Evidence / limitations

- GitHub exact-branch re-fetch/static cross-file contract assertions: PASS (new symbol declared+implemented, old numeric marker compile-guarded, tests use bounded marker, executable not linked to live Studio).
- Independent SHA-256 of the existing canonical example: `377c15b69c76f48cedb27ad038eb33b8bf467028ad427928a96a2d7577ce833d`. This is pure algorithm verification, **not** a Windows Native runner result.
- Opt-in Native ACK-05 test compile and run: **NOT_RUN**.
- Retained 5 Native G5 CTests, SHA vectors on Windows, Python regression, ABI20/Windows Release: **NOT_RUN at updated HEAD**.
- Actual durable read-only recovery IPC with post-effect/pre-effect ACK injection: **NOT_IMPLEMENTED**.
- Trusted Host-bound actor identity, UI/V1/Agent shared routing, physical frame acceptance, crash recovery: **NOT_IMPLEMENTED / NOT_RUN**.
- G5 Native writer: **HOLD**. No main merge or deploy.

**Important:** The internal command marker operates in an in-memory synthetic bus. Production transport should durably stage the exact command identity prior to posting a mutation and must never clear a marker merely because a receipt is absent. It also must not trust actor identity or request hashes asserted by caller-controlled JSON. A full Studio Host integration needs one owner-thread linearization path and physical Windows fault injection.

## Next gate

Run opt-in native tests with `STUDIO_G5_ACTOR_RED_FIRST=ON` on a connected Windows Host and retain all denial assertions. Then independently review the transaction/receipt/marker atomicity at COMMIT, UNDO and REDO and the ownership boundary of the real recovery transport. Only after proven Host identity, IPC quarantine and FrameCoordinator acceptance can a separate decision lift the G5 writer HOLD.
