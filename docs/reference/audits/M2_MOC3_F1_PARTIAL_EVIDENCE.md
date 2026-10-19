# M2 MOC3 F1 partial implementation evidence

**Historical checkpoint:** superseded by [F1 local acceptance](../../completed/audits/M2_MOC3_F1_LOCAL_ACCEPTANCE.md). The HOLDs below describe the earlier partial implementation, not the current trusted-local F1 verdict.

**Date:** 2026-10-18. **Branch:** `m2/moc3-p1-snapshot`. **Parent HEAD:** `472aa1624a70710b8f2498dcd6d2e20fa7fe64b4`; this packet describes an uncommitted working-tree diff. **Disposition:** F1 PARTIAL / full F1 HOLD. F2 host pixels, F3 linked Physics3, F4 activation and D06/D08 physical loss evidence remain HOLD.

## Implemented boundary

The optional independent MOC3 build remains default OFF. The private Python preflight reads only manifest-linked MOC/PNG/Physics descriptor files beneath a selected local root, checks reference containment and configured budgets, and decodes a limited RGB8/RGBA8 PNG subset on CPU. The native `Session::update_complete` creates full default-based frames and publishes immutable snapshots transactionally. A private stdin/stdout bridge keeps the evaluator on one owner thread and emits coarse pose digests for diagnostics. The agent-callable `F1-CPU-PREFLIGHT` suite runs generated fixtures. `F1-LOCAL-REPLAY` invokes the local private fixture through the same API/window coordinator, with no model path or digest in its API result. The ignored descriptor directory and exclusive-created token file were restricted to the launching Windows account; the token file was observed with only that account's ACL entry after the elevated local harness launch and was removed on clean exit.

## Checks run

| Check | Result | Meaning |
| --- | --- | --- |
| `python -m unittest discover -s tests/moc3 -p test_package_admission.py -v` | PASS, 5 tests | Generated manifest/PNG positive and negative preflight. |
| `python -m unittest discover -s tests/moc3 -p test_harness_api.py -v` | PASS, 7 tests | Loopback API, event/cancel/idempotency and explicit F1 HOLD behavior. |
| `ctest --test-dir build/moc3-t0-verify -C Debug -R 'moc3_(t0_harness_api\|f1_cpu_preflight)' --output-on-failure` | PASS, 2/2 | Registered prep-only tests. |
| `cmake --build build/moc3-p1-native --config Release --target ars_moc3_harness_bridge ars_moc3_snapshot_tests` | PASS | Optional independent evaluator and bridge build using locally pinned Purism Core. |
| `ctest --test-dir build/moc3-p1-native -C Release -R moc3_independent_snapshot --output-on-failure` | PASS, 1/1 | Public negative MOC/path snapshot checks. |
| Opt-in private native snapshot test | PASS | Changed pose, complete-frame reset/repeat, invalid parameter and wrong-thread preservation. No model-derived values exported. |
| `python scripts/moc3_local_replay.py` | PASS | Private CPU package preflight and bridge changed/reset pose check. |
| Loopback `F1-LOCAL-REPLAY` through `scripts/moc3_harness_client.py` protocol | PASS | Run reached `PRIVATE_CPU_PACKAGE_AND_POSE_ONLY`; queued, started and complete events observed. |
| `python -m py_compile` for F1/harness scripts; `git diff --check` | PASS | Syntax and tracked diff whitespace. |

The private fixture is licensed for local use only and remains ignored under `local-assets`; neither its bytes nor derived hashes, images, counts or timing measurements belong in this document. The bridge pose digest is a repeatability witness, not an authored reference or pixel oracle.

## Remaining F1 exit conditions

1. **Atomic package/evaluator ownership:** the Python preflight keeps MOC bytes while the native bridge still reopens a MOC path. A file swap between these steps is possible. Build a handle/blob-bound handoff and show failed replacement preserves the prior package and snapshot.
2. **Complete resource policy:** measure peak parser/decoder/evaluator/snapshot allocations and in-flight generations, including failure injection. Current Python PNG subset does not establish color-profile equivalence or a portable memory peak.
3. **Feature inventory and fixture portability:** fully report unsupported linked behaviors and drawable/texture requirements before F2. Public CI has generated manifest/PNG fixtures but no redistributable real MOC3 evaluator fixture.
4. **Host interface:** admitted texture slots and snapshots are not connected to the host D3D11 offscreen renderer. Thus `ADMIT` and `POSE-REPLAY` as full harness sessions remain HOLD; the private diagnostic does not unlock F2.

No F* stage or core acceptance gate is closed by green CI alone. F2 needs a frozen multi-atlas/mask/blend pixel contract and clean readback on WARP/real hardware; F3 needs manifest-linked Physics3 execution and fixed-clock host pixels. D06/D08 physical loss remains HOLD until a reset-capable GPU runner is available.
