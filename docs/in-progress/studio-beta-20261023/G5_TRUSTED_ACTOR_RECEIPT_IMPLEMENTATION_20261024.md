# G5 Trusted Actor / Bound Receipt — GitHub Implementation Checkpoint

**Date:** 2026-10-24  
**Repository:** `boxs-51/Avatar_Runtime`  
**Branch:** `exp/g5-local-cas-transactions`  
**Baseline before this round:** `646ed1f7ce221179502ff4472667aec1831e3429`  
**Scope:** Only G5 internal test-only bus, transaction receipt representation, hashing, CMake and tests. No public ABI20, no model source/geometry, no Studio UI mutation, no production V2 writer, no `main` merge.

## Contract and changes

- **G5-ING-01 / G5-ING-02:** Add `G5TrustedActor { ingress, owner }` to the internal command bus. Compose a separate 64-bit transaction owner from trusted ingress and session actor ID. A caller-supplied `G5BusCommand.ingress` that disagrees with the Host-bound trusted actor is denied. Reject zero and out-of-range owner IDs.
- **G5-ING-03:** New actor-keyed bounded quarantine table (16 slots) allows Agent A to remain quarantined while distinct Agent B can operate, provided the Native draft lease is free. An unknown original request cannot clear its own quarantine; a foreign actor cannot reconcile it.
- **G5-ACK-02:** Extend `G5Receipt` with operation and `payload_sha256[32]`. The bus calculates the digest locally from a canonical, versioned big-endian command serialization: ingress, trusted actor, operation, session, model generation, expected document revision, draft ID, request ID and operation-relevant primitive arguments. No caller-provided digest or C struct memory/padding is trusted.
- **G5-ACK-03:** A committed transaction is recorded before returning an ACK, and the same owner-thread dispatch binds receipt metadata before the externally visible return. A missing or mismatched internal receipt produces UNKNOWN instead of a false successful ACK. This is volatile single-thread **in-process** behavior, not crash-durable exactly-once execution.
- **G5-COMPAT-05:** The old category-only `g5_bus_submit/recover/reconcile/mark_unknown` entry points are now compiled only with `G5_TEST_ONLY_UNBOUND_BUS`; the macro is set solely for the two existing test executables which require it. Future Studio/Agent routing must use trusted API only.
- **Privacy:** The SHA-256 payload encoding is limited to command envelope metadata, not raw geometry, model paths, images or model binaries. Real Host identity must be derived from the trusted connection; a C struct alone is not an OS security boundary.

## Modified/added source

- `tools/avatar-studio/studio_g5_bus.h`
- `tools/avatar-studio/studio_g5_bus.c`
- `tools/avatar-studio/studio_g5_txn.h`
- `tools/avatar-studio/studio_g5_sha256.h` (new)
- `tools/avatar-studio/tests/studio_g5_actor_receipt_redfirst.c` (expanded opt-in)
- `tools/avatar-studio/tests/studio_g5_sha256_tests.c` (new)
- `tools/avatar-studio/CMakeLists.txt`

**Test selection:** SHA-256 vector target is registered as normal G5 CTest; actor-bound contract is opt-in via `-DSTUDIO_G5_ACTOR_RED_FIRST=ON`. G5 writer is still **not** linked into the production Studio executable.

## Evidence status

| Check | Status | Evidence/limits |
|---|---|---|
| Pushed GitHub branch and file integrity | PASS | Connector fetched updated files; verified all four trusted bus symbols and compile gating |
| SHA-256 algorithm three independent vectors | PASS, portable Linux GCC | Compiled a local copy of the exact pushed SHA-256 helper (contents FNV32 = `f6192bfc`) with `gcc -std=c11 -Wall -Wextra -Werror -pedantic`; empty, `abc`, quick-brown-fox vectors matched |
| Native actor and receipt RED-first test | **NOT_RUN** | Windows Host tunnel disconnected and GitHub Actions quota unavailable; this file is not proof of GREEN |
| Existing five G5 Windows Native CTests | **NOT_RUN on new HEAD** | Prior local historical 5/5 PASS predates the changes in this checkpoint |
| Python regression and Windows ABI20 CTest | **NOT_RUN on new HEAD** | Do not infer GREEN from historical results |
| Actual physical IPC lost-ACK/crash recovery | **NOT_IMPLEMENTED** | Only in-memory quarantine/receipt has been extended |
| Actual Studio GUI + Agent concurrent writer routing | **NOT_IMPLEMENTED** | Existing legacy UI/V1 paths not migrated |
| Accepted solver/GPU/frame fence | **NOT_IMPLEMENTED** | G5 writer and frame acceptance remain HOLD |

## Required next gates

1. Compile opt-in `studio_g5_actor_receipt_redfirst` on Windows with `STUDIO_G5_ACTOR_RED_FIRST=ON`; fix any real compile/runtime regressions without changing negative oracles.
2. Run the original five G5 Native CTests, new SHA vector CTest, default Python, Release CTest/ABI20 and an independent source/security review.
3. Bind trusted actor identity to an actual Host-managed local session (not client JSON) and route every UI/V1/Agent edit via the same owner-thread command bus; prove direct legacy edits cannot bypass active lease.
4. Implement a real separately isolated read-only recovery IPC path, inject pre/post-effect ACK loss, and verify crash/restart and receipt lifetime. Prevent blind retry.
5. Validate FrameCoordinator acceptance and physics/GPU fences before returning APPLIED; keep G5 Native writer disabled and no main merge until these gates pass.

**Delivery decision:** internal trusted-actor code **IMPLEMENTED, NATIVE VERIFICATION PENDING**. Full G5 release and Native writer **HOLD**. No merge or deploy.
