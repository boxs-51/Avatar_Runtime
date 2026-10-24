# G5 Windows Host Recovery IPC Fixture — RED-first and physical two-process evidence

**Date:** 2026-10-24
**Repository:** `boxs-51/Avatar_Runtime`
**Branch:** `exp/g5-local-cas-transactions`
**Baseline:** `e491172ae2ff256ed8f263974f10c017ae3f5a81` (fast-forwarded locally from `f41f9a3` with clean worktree).
**Host:** Windows 10; Computer Bridge `beta`; `USER_CONTEXT_NO_OS_SANDBOX`.
**Scope:** isolated model-free Native G5 recovery transport *test fixture*, not Studio product IPC or a production writer. No raw Cubism model assets used.

## Goals and constraints

- Verify that the previous G5 trusted-actor and bound receipt changes from GitHub compile and pass on a real Windows Native toolchain.
- Verify pre/post-effect lost ACK and unknown outcome using two **separate operating-system processes** and independently published filesystem mutation and read-only recovery request slots.
- Distinguish a volatile owner receipt and restarted session epoch, retain quarantine until correctly reconciled, and never retry based on a missing receipt.
- **MUST NOT:** alter public ABI20, link G5 writer into `avatar_studio`, enable V2 mutation, merge main, use a real `.moc3`, or present the fixture as the Studio IPC security boundary.

## Test infrastructure

- `tools/avatar-studio/tests/studio_g5_ipc_fixture.c`: test-only Native DLL exposing synthetic Rig G5Bus capabilities. Uses a fixed trusted test actor and a per-process randomly supplied session epoch. It can commit and deliberately lose ACK, mark pre-effect uncertainty, query COMMITTED/UNKNOWN, attempt a quarantined mutation, or reconcile an original request.
- `scripts/test_studio_g5_process_recovery.py`: Windows test driver launches separate Python child process, loading the real Native DLL via `ctypes`. Fixed ephemeral request/response slots reside only in a newly created directory under ignored `build/studio-user/`. The child owner serially processes requests from physically distinct `mutation/` and `recovery/` slots. The recovery slot is strictly read-only.
- `tools/avatar-studio/CMakeLists.txt`: opt-in `STUDIO_G5_IPC_FIXTURE=ON`; standalone DLL/CTest test, with the option **OFF** by default. The fixture and G5Bus remain unlinked from the production `avatar_studio` executable.

**Security scope:** This uses test-only file IPC with synthetic identity. It is not the authenticated Studio Host IPC, provides no process sandbox, and does not prove protection against a malicious same-user process. The test directory is an ignored disposable fixture created by the test driver, not a user asset workspace.

## Observed test sequence

1. Fetch `origin/exp/g5-local-cas-transactions`, fast-forward current local branch to exact `e491172`, verify clean worktree.
2. Configure `STUDIO_G5_ACTOR_RED_FIRST=ON`, build eight Native G5 targets with MSVC Release. **8/8 PASS**, including SHA-256 vectors and opt-in original command binding.
3. `python -B -m pytest -p no:cacheprovider -q tests/moc3 --tb=short`: **211 PASS, 18 SKIP, 204 subtests PASS**.
4. Build `avatar_studio` Release **PASS**; top-level Windows CTest **10/10 PASS**, including C11/C++20 ABI20.
5. Create test-only DLL and two-process file IPC driver. Initial post-effect lost ACK fixture **PASS**: parent receives no mutation ACK, recovery returns COMMITTED@8, UNKNOWN ID remains UNKNOWN, mutation remains blocked until valid reconciliation.
6. Extend fixture to pre-effect uncertainty and crash/restart. **PASS**: before owner commit, response is UNKNOWN and remains quarantined; after restart volatile receipts are absent, no replay or durable exactly-once promise.
7. Add a strict RED-first check that session epoch must change between three process lifetimes. **Observed RED** before fix: `ValueError` parsing hard-coded readiness token `ready`. Fix: OS-generated 63-bit epoch per owner process, injected into Native G5Bus and emitted to the fixture readiness sentinel. **Observed GREEN**: all three epochs are distinct.
8. Register process recovery as opt-in CTest. Command `ctest --test-dir build/avatar-studio -C Release -R ^g5_ --output-on-failure`: **9/9 PASS**, including physical two-process fixture.

Observed success marker:
`G5_PROCESS_RECOVERY_PASS channels=2 post_effect=COMMITTED pre_effect=UNKNOWN restart=volatile no_blind_replay=1`

## Evidence tiers and outstanding gates

| Gate | Status | Evidence or limitation |
|---|---|---|
| Native synthetic transaction and trust-bound receipt core | PASS (Windows) | 8 Native G5 unit/integration CTests |
| Two-process file transport, post-effect lost ACK | PASS (test fixture) | recovery reads COMMITTED@8 while mutation ACK intentionally absent |
| Pre-effect unknown and no blind replay | PASS (test fixture) | UNKNOWN receipt and bounded mutation quarantine |
| Crash/restart and epoch change | PASS (test fixture) | volatile receipt lost, new random epoch, no durability claim |
| Public SDK ABI20 | PASS | 10/10 top-level CTests; no public ABI source changes |
| Real Studio owner-thread command bus routing (GUI/V1/Agent) | NOT_IMPLEMENTED | legacy UI/Agent direct mutation remains outside G5 controller |
| Trusted Host ingress/session authorization | NOT_IMPLEMENTED | test fixture actor fixed and synthetic; OS inherited user context is not sandbox |
| Production isolated read-only recovery IPC | NOT_IMPLEMENTED | test-only filesystem lanes must not be reused as a security-boundary API |
| GPU/XPBD FrameCoordinator acceptance | NOT_IMPLEMENTED | synthetic `applied` boolean does not prove frame acceptance |
| Independent review for newly authored fixture | NOT_RUN | source and test evidence need separate independent review |
| G5 Native Studio writer | HOLD | disabled and unlinked |

## Delivery decision

G5 **isolated process-level recovery transport evidence = PASS within fixture scope**.
End-to-end Studio G5 functionality = **NOT COMPLETE / HOLD**. No production writer, automatic replay, main merge or deploy authorization is implied. The next stage is design/implementation of a verified Host-owned actor session adapter and real Studio recovery lane, then phased red-first migration of all UI/V1/V2 mutation paths through one Native owner-thread dispatcher, plus actual frame acceptance and fault-injection evidence.
