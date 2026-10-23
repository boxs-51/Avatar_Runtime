# G5 Native Local T2 — transaction core checkpoint (2026-10-24)

## Status and scope
- Repository: `boxs-51/Avatar_Runtime`; Windows 10 Computer Bridge workspace alias `beta`.
- Current branch: `exp/g5-local-cas-transactions`.
- Unmodified committed HEAD at start: `56cc82968336e99bdc45712979be8ffa3e8769d7`.
- **LOCAL WORKING TREE ONLY**. Owner explicitly prohibited push without subsequent confirmation. No GitHub push, PR, merge, deployment, or production writer activation is allowed in this round.
- Requirement: continue implementing G5 locally, preserve ABI20, private model data and earlier G4 behavior. No claim that partial G5 T2 is complete end-to-end.
- Backup of existing CMake file and first-pass G5 module/tests under ignored `build/studio-user/g5-native-t1-backup/`. Pre-existing dirty worktree was clean at start.

## Implemented Native C transaction core
New internal files (no public SDK symbols):
- `tools/avatar-studio/studio_g5_txn.h`
- `tools/avatar-studio/studio_g5_txn.c`
- `tools/avatar-studio/tests/studio_g5_txn_tests.c`
- `tools/avatar-studio/tests/studio_g5_txn_adversarial.c`
- `tools/avatar-studio/tests/studio_g5_txn_edit_ops.c`
- Scoped modification: `tools/avatar-studio/CMakeLists.txt` registers three test-only executables and standalone CTest tests; `avatar_studio` application target is NOT linked to this G5 core.

Properties currently backed by Native (synthetic, standalone) tests:
1. A single owner-thread private draft and exclusive writer lease; other owner receives BUSY.
2. Fence by `session_epoch`, `model_generation`, `document_revision`; a stale lease/model/session cannot change live Rig.
3. Staged anchor, beam, parent, weight, cross-link operations, using existing Rig V4 validation functions. No live state changes before a validated commit.
4. Explicit validate, atomic in-memory commit with pre-reserved receipt and history capacity, abort discarding only private draft.
5. Monotonic document revision, even when undo/redo makes content equal to prior state (ABA). Stale CAS rejected; a new branch clears redo.
6. Immutable bounded `(session_epoch, request_id, owner)` receipt lookup for committed transactions; unknown entries return UNKNOWN, never positive evidence that no effect occurred. Collision prevents retrying a previous ID (including identical ID).
7. Fixed 16-entry undo history and 32-entry receipt index, fail-closed when full. Checked 63-bit revision overflow; no wraparound.
8. A separate `applied_revision` that does not change on COMMITTED; only an explicit apply call with accepted frame boolean advances it. **This is an internal test primitive, not frame/GPU acceptance proof.**

All external origin or authorizations are synthetic numeric test IDs. Integration must derive identity from the trusted local ingress and must validate cross-link target vertex metadata against the live generation-fenced model, not trust a caller-provided value.

## RED-first evidence
- Initial new native target could not configure because `studio_g5_txn.c` was absent: source-absence baseline RED, not a runtime defect.
- Tests were written before the transaction core and gained GREEN after implementation.
- Security contract revision added `session_epoch` and `model_generation` checks to every draft operation; prior source failed to compile with the stricter tests (expected RED), corrected source GREEN.
- Staged beam/parent/weight/cross operations were first RED at link time (4 unresolved symbols); then GREEN after new implementations.

## Actual commands and observed results
- `cmake --build build/avatar-studio --config Release --target studio_g5_txn_tests studio_g5_txn_adversarial studio_g5_txn_edit_ops --parallel 2`: PASS.
- `ctest --test-dir build/avatar-studio -C Release -R ^g5_txn_ --output-on-failure`: 3/3 PASS, Native Windows Release. Test data is synthetic, with no private model load or UI interaction.
- `python -B -m pytest -p no:cacheprovider -q tests/moc3 --tb=short`: 208 PASS / 18 SKIP / 204 subtests PASS.
- `ctest --test-dir build -C Release --output-on-failure --timeout 25`: 10/10 PASS, including C11/C++20 ABI20.
- `git diff --check`: PASS.
- `git diff --name-only -- include/ars/ars.h src/abi CMakeLists.txt scripts/avatar_toolkit_v2.py scripts/studio_agent_api.py tools/avatar-studio/studio.c`: empty (new G5 is test-only internal).
- MSVC `/W4` on standalone G5 Native test targets: build PASS. Warnings from existing `studio_rig.c` `fscanf` remain visible; no warning suppression.

## Open gates — NOT complete G5

| G5 gate | Assessment | Required next evidence |
|---|---|---|
| G5-CORE Draft/CAS/revision/undo | GREEN **native synthetic only** | Independent review; isolate/private owner-thread state under real Studio |
| UI/Agent unified EditCommandBus | NOT_IMPLEMENTED | Route all UI, V1 and V2 edits through one Native owner; verify no bypass |
| Explicit editor/session authorization | NOT_IMPLEMENTED at ingress | Trusted identity binding, session/lease security; no user-controlled owner ID |
| Lost ACK `draft.status` recovery IPC | NOT_IMPLEMENTED | Separate read-only recovery channel while ordinary writer queue quarantined; physical fault injection |
| Idempotent payload/receipt index | PARTIAL | Bind receipt to original operation+payload hash; durable lookup/lifetime decisions |
| Native CAS model reload and GUI simultaneous edits | NOT_RUN | Physical UI+Agent race and same-frame ABA matrix |
| Native accepted frame / solver / GPU fence | NOT_IMPLEMENTED | FrameCoordinator proof; cannot mark APPLIED based on test boolean |
| G5 V2 user-facing editing | DISABLED/HOLD | Schema/API approval and safe owner bus + recovery+frame gates |
| Crash/restart persistence | NOT_RUN | Explicit crash, restart, uncertain outcome; no exactly-once claim |
| ABI20 public SDK change | NONE | Keep it unchanged |
| GitHub publishing | PROHIBITED | Await express Owner push authorization |

Do not describe this first stage as complete G5 T2 or approve a G5 writer based on isolated tests. No private model assets or screenshots were read or exported by these new Native tests.

## Follow-on implementation order (local only)
1. Independent source/security review of internal G5Txn with injection of target metadata, ownership, memory bounds and rig validation.
2. Typed Native EditCommandBus around one owner; migrate UI/legacy V1 edits or deny them during active lease; prove no bypass before enabling V2 writes.
3. Dedicated bounded, identity-scoped read-only recovery transport and post-effect lost-ACK replay oracles.
4. FrameCoordinator acceptance proof, undo/redo physical and UI/Agent concurrency matrix.
5. Mark Gate PASS only with exact physical evidence. Do not push/merge without explicit instruction.
