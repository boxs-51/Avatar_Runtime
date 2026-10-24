# G5 ACK Recovery — Fail-closed Receipt Output (RED-first follow-up)

**Date:** 2026-10-24  
**Repo:** `boxs-51/Avatar_Runtime`  
**Branch:** `exp/g5-local-cas-transactions`  
**Baseline inspected:** `c8887ed9e27df09c66bed4fde31b3c30d9f6a6b3`  
**Scope:** Native G5 internal recovery only. No Studio mutation activation, public ABI20 change, private model access, `main` merge or deployment.

## Finding G5-ACK-04 — stale response memory after denial

The initial `g5_txn_receipt` did not clear the caller's `G5Receipt` buffer before returning `G5_STALE_SESSION` on a wrong owner/session. A caller that reused a buffer from a previous successful query could thus retain an outdated COMMITTED outcome, revision or payload digest after a rejected lookup. `g5_bus_recover_trusted` also returned early for invalid trusted actors without clearing the provided buffer.

This is a **fail-closed internal API concern**, not demonstrated leakage through a live external IPC endpoint: the G5 trusted bus and recovery lane are not linked into the shipping Studio writer.

## Targeted RED-first oracle

Added tests before the source changes:

- `studio_g5_txn_adversarial.c`: after a successful owner-scoped lookup, query the same request as a foreign owner and assert `owner==0`, `request_id==0`, `revision==0`, `operation==0`, `outcome==0`.
- `studio_g5_actor_receipt_redfirst.c`: reuse the output of a previously valid receipt including SHA-256, then query as a foreign Agent or invalid actor. Denial must clear all metadata and every digest byte, without changing the recorded transaction. A permitted owner can still recover its immutable receipt afterward.

**Execution note:** Source-before-fix inspection verifies the absence of output scrubbing; these tests were **not executed** against the old build, because Computer Bridge reported `Tunnel-client has not been seen for 300 seconds` and GitHub Actions had quota restrictions. Label the intended failing behavior **EXPECTED RED / NOT_RUN**, not an observed RED.

## Implementation

- `studio_g5_txn.c::g5_txn_receipt` clears any nonnull caller output before identity checks or receipt lookup.
- `studio_g5_bus.c::g5_bus_recover_trusted` clears any nonnull output before the early invalid-actor/ingress rejection; the common transaction-layer lookup also initializes the output.

This is a narrow defense-in-depth change, with no changes to committed transaction state, canonical SHA-256 payload serialization, writer lease or G5 schema.

## Verification and held gates

| Gate | Status | Evidence |
|---|---|---|
| GitHub exact-file re-fetch and diff scope | PASS | Only `studio_g5_txn.c`, `studio_g5_bus.c` and two Native G5 test files in this follow-up |
| Source-level deny-output initialization | PASS | Both functions initialize nonnull output |
| Known SHA-256 canonical digest | PASS, pure independent calculation | SHA-256("ARS-G5-REQUEST-v1" + 10 big-endian uint64 semantic fields) matches the 32-byte expected fixture in opt-in test |
| New Native denied-output tests | **NOT_RUN** | Windows Host disconnected |
| G5 default five Native CTests and opt-in trusted actor CTest | **NOT_RUN on updated HEAD** | Require Windows Release runner and full retained assertions |
| UI/Agent physical routing, read-only recovery IPC, crash test, frame acceptance | NOT_IMPLEMENTED / NOT_RUN | Required before writer activation |
| G5 Native writer | HOLD | Unlinked from shipping Studio and no new capability admission |
| Independent reviewer | NOT_RUN | Review new C diff after Native evidence |

**Next step:** Recover Windows Host or an equivalent authorized Windows runner, enable `STUDIO_G5_ACTOR_RED_FIRST=ON` in an isolated build, execute RED → GREEN and retained suite, then continue the Host-bound identity and actual read-only recovery transport implementation. Do not publish a PASS claim without the physical results.