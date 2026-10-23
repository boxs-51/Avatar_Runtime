# G5 Local Phase Next — Unified Internal Edit Bus and Recovery Protocol

**Date:** 2026-10-24  
**Repository:** boxs-51/Avatar_Runtime  
**Local workspace:** Windows Computer Bridge alias `beta`  
**Branch:** `exp/g5-local-cas-transactions`  
**Committed HEAD throughout:** `56cc82968336e99bdc45712979be8ffa3e8769d7`  
**Publish constraint:** LOCAL ONLY — no GitHub push, PR, merge or deployment without a new explicit owner command.  
**Runtime gate:** V2 G5 mutation DISABLED/HOLD; public ABI20 unchanged.

## Requirements covered and evidence

REQ-G5-ROUTE: Use one owner-thread transaction authority for candidate UI / legacy V1 / Agent requests. Implemented a standalone `G5Bus` on top of the earlier `G5Txn`. Sources are typed `G5_UI`, `G5_LEGACY` and `G5_AGENT`; the trust boundary is important: the Native ingress must set the source. The source enum is **not itself authentication**, and these commands are not yet linked to Studio. A legacy anchor edit is represented as a single bounded begin→edit→validate→commit step; an active draft makes it BUSY. Stale revision and stale session/model reject. Current test: `studio_g5_bus_recovery_tests.c`, Windows Native Release PASS.

REQ-G5-ACK: Query the original transaction receipt when an ACK is lost. The bus now exposes a **read-only recovery function** independent of its per-ingress mutation quarantine. `g5_bus_mark_unknown` models a transport timeout; readback of immutable Native committed receipt is possible but does not clear quarantine. `g5_bus_reconcile` clears only the same request marker when an owner-scoped terminal receipt exists. UNKNOWN leaves quarantine intact; no automatic replay. This is still an **in-memory Native scenario**, not physical IPC fault injection.

REQ-G5-WIRE: Strict recovery payload parser. New `studio_g5_recovery_wire.c/h` implements a bounded, binary-length-aware decoder supporting only `STATUS <positive decimal request-id>`. The trusted session/ingress must arrive out of band from a future Host IPC boundary, never from the wire. Reject command injection, trailing newline/extra text, zero/leading zeros, overflow, embedded NUL, wrong session/ingress and short response buffers. The reply contains only outcome, request ID and document revision. No private model data or paths. Physical Windows Native standalone decoder test PASS. **No filesystem/NPIPE/IPC endpoint has been created yet.**

REQ-G5-NEGATIVE: Three default Python regression guards `test_studio_g5_writer_gate.py` ensure no G5 source linked into live `avatar_studio`, V2 `mutating_commands_enabled=false`, and a draft.begin mutation is rejected before any bridge call. These guard against accidental enablement.

## New local files

- `tools/avatar-studio/studio_g5_bus.h`
- `tools/avatar-studio/studio_g5_bus.c`
- `tools/avatar-studio/studio_g5_recovery_wire.h`
- `tools/avatar-studio/studio_g5_recovery_wire.c`
- `tools/avatar-studio/tests/studio_g5_bus_recovery_tests.c`
- `tools/avatar-studio/tests/studio_g5_recovery_wire_tests.c`
- `tests/moc3/test_studio_g5_writer_gate.py`

The existing local `tools/avatar-studio/CMakeLists.txt` is further extended with two new standalone Native test executables registered as CTest. The previous local G5Txn source/tests remain uncommitted.

## Test record / proof tier

- RED-first owner-bus target without source: CMake source-missing failure — structural RED, not a Native race reproduction.
- After implementation: `studio_g5_bus_recovery_tests.exe` PASS with simulated post-commit ACK loss, owner-scoped readback and legacy path lease.
- RED-first recovery-wire target without source: CMake source-missing failure — structural RED.
- After implementation: `studio_g5_recovery_wire_tests.exe` PASS with malformed message, overflow, wrong identity, NUL, short buffer.
- `ctest --test-dir build/avatar-studio -C Release -R ^g5_ --output-on-failure`: 5/5 PASS (3 previous core/edit/fault cases + 2 new bus/recovery cases). Native Windows physical executable, but **synthetic Rig, simulated transport and no GUI/GPU**.
- `python -B -m pytest -p no:cacheprovider -q tests/moc3 --tb=short`: previous run 208 PASS / 18 SKIP / 204 subtests PASS before the 3 newly added G5 guard tests. Re-run final after this report.
- `python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_g5_writer_gate.py --tb=short`: 3 PASS.
- `ctest --test-dir build -C Release --output-on-failure --timeout 25`: 10/10 PASS incl public ABI20.
- `cmake --build build/avatar-studio --config Release --target avatar_studio ...`: PASS; G5 files are not linked into `avatar_studio`.
- `git diff --name-only -- include/ars/ars.h src/abi CMakeLists.txt scripts/avatar_toolkit_v2.py scripts/studio_agent_api.py tools/avatar-studio/studio.c`: empty.

## Security risks and gates retained

1. **Ingress actor identity**: `G5Ingress` numeric type must be assigned by a verified Studio Host command adapter; user-controlled JSON cannot supply privileged UI identity. Not integrated; G5 T2 writer still HOLD.
2. **Owner-thread concurrency**: the bus is single-thread-only and not safe to call concurrently from arbitrary OS threads. Future ingress must queue and execute on the Studio owner thread; real simultaneous GUI/Agent and same-frame ABA tests NOT_RUN.
3. **Post-effect lost ACK**: simulated missing ACK and stable owner receipt lookup are green; real separate, isolated IPC transport while mutation queue is quarantined is NOT_IMPLEMENTED. No durable exactly-once or crash/restart semantics.
4. **Receipt payload identity**: core indexes numeric request ID + scoped owner/session but currently does **not** store a canonical original payload digest/opcode. All duplicate IDs fail closed, but the final correlation/anti-confusion gate is still OPEN before external exposure.
5. **Model validation**: cross target vertex count in the internal transaction core can be supplied as a number; it must be checked against actual model generation/target metadata when integrated, not accepted from untrusted ingress.
6. **COMMITTED vs APPLIED**: the core tracks separate revisions, but real GPU/fence acceptance and solver generation are not connected. `accepted` in the synthetic core is not physical proof.
7. **Legacy bypass**: old UI and V1 direct writes still exist in `studio.c`. This experimental command bus does not silently secure them. Direct paths must be routed through one authority or blocked under an active G5 lease before writer activation.
8. **Host OS**: Computer Bridge runs as inherited host user with no OS sandbox; parser correctness is not adversarial same-user process authorization.
9. **Independent review**: no independent reviewer has inspected these new local uncommitted G5 changes.
10. **No GitHub publishing**: user explicitly forbids push until a separate confirmation. No action should stage/commit/push or merge without it.

## Result and next stage

Scoped Native **internal routing and read-only recovery handler**: PASS in synthetic standalone Native Windows tests.
G5 actual Studio UI/Agent E2E: NOT IMPLEMENTED / NOT RUN.
G5 Native V2 writer activation: HOLD.
Recommended next local stage: add a trusted ingress/session adapter and a physically isolated read-only IPC recovery lane with process-level ACK-loss injection; then integrate UI/V1/V2 via the same owner-thread command bus, prove no bypass, attach FrameCoordinator acceptance fences, and only then consider lifting the gate. All must stay local unless owner explicitly authorizes publication.


## Final exact local verification

- Windows branch local HEAD: 56cc82968336e99bdc45712979be8ffa3e8769d7.
- python -B -m pytest -p no:cacheprovider -q tests/moc3 --tb=short: **211 passed, 18 skipped, 204 subtests passed** (three additional default G5 writer-gate controls).
- ctest --test-dir build/avatar-studio -C Release -R ^g5_ --output-on-failure: **5/5 PASS**.
- ctest --test-dir build -C Release --output-on-failure --timeout 25: **10/10 PASS**.
- Full Windows Release avatar_studio build PASS with standalone bus/recovery sources still not linked into Studio.
- git diff --check PASS; public ABI20 and active V2/Studio source paths unchanged.
- Evidence is from this local uncommitted working tree, not merely the older committed HEAD.
- No push, commit, PR or merge performed.
