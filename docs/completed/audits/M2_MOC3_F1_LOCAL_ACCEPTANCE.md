# M2 MOC3 F1 trusted-local CPU acceptance

**Verdict: PASS for F1 trusted-local CPU scope.** Actual execution on this Windows x64 machine with the authorized local model is the primary acceptance evidence, as instructed by the owner. GitHub Actions is corroborating only; no Actions run or public MOC3 fixture is claimed by this packet. F2 host pixels, F3 linked Physics3, F4 activation and real physical D06/D08 remain separate HOLDs.

The tested worktree is above parent HEAD `472aa1624a70710b8f2498dcd6d2e20fa7fe64b4` on `m2/moc3-p1-snapshot`. Changes are uncommitted. The [source-content manifest](../../reference/audits/M2_MOC3_F1_LOCAL_SOURCE.json) pins every relevant code/test file, the actual native binaries and the Purism dependency revision. A parent HEAD alone is insufficient to identify this run. The prior [partial checkpoint](../../reference/audits/M2_MOC3_F1_PARTIAL_EVIDENCE.md) is historical.

## Implemented result

Manifest references resolve relative to the manifest's own directory beneath the authorized root. The Windows opened-file handle is checked before reading bytes; reparse/ambiguous paths and duplicate file identities reject. CPU admission owns immutable MOC bytes, the ordered decoded RGBA8 atlases and linked Physics3 descriptor bytes. All optional linked behavior execution is explicitly disabled; requested unsupported execution rejects.

The native evaluator receives those exact MOC bytes and actual atlas count, with no path reopen. `AdmittedSession` prepares one replacement worker while the old worker/package remains active. Publication exchanges the complete prepared package/evaluator pair only after native validation, feature inventory and the measured-memory check succeed. Candidate failures leave the prior worker, package, generation and immutable snapshot unchanged. Complete parameter frames reset unspecified values to model defaults; snapshot comparison covers every drawable field rather than relying only on a geometry hash.

The optional feature remains default OFF, uses the pinned independent Purism implementation, and does not change the public ARS ABI or v0.1 asset budgets. It executes CPU work only; no texture upload, D3D11 submission, host frame or Physics3 execution is part of F1.

## Direct local evidence

`python scripts/moc3_local_replay.py` completed with exit 0 and `F1_PRIVATE_LOCAL_REPLAY_PASS` on the final source manifest. Its ignored private JSON packet contains the source/binary/input provenance, per-case results, native feature inventory and Windows process lifetime memory peaks. All 11 required local groups passed:

| Requirement / case | Direct witness | Result |
| --- | --- | --- |
| F1-L01 immutable package graph | Declared ordered atlases, immutable byte lengths, disabled optional features and separate resource policy. | PASS |
| F1-L02 blob and atlas binding | Exact admitted MOC blob reaches the native worker; drawable texture indices resolve inside the actual package atlas count. | PASS |
| F1-L03 full drawable replay | Default → changed pose → default → repeat compares all drawable geometry, UVs, ordering, opacity, mask/blend/color state. | PASS |
| F1-L04 rejected input | Invalid/huge/nonfinite parameter, duplicate ID, stale package generation and wrong owner thread do not publish. | PASS |
| F1-L05 snapshot allocation rollback | Every drawable-copy checkpoint fails with the old snapshot retained; a later valid pose/reset succeeds. | PASS |
| F1-L06 failed replacement | Native creation checkpoints, corrupt MOC, too-few atlases and unsupported requested behavior retain the old package, worker and generation. | PASS |
| F1-L07 bootstrap cleanup | Injected response-thread startup failure terminates the real newly spawned worker while the old session remains usable. | PASS |
| F1-L08 memory rejection | A deliberately lowered candidate acceptance limit rejects before publication, retaining the old session. | PASS |
| F1-L09 successful replacement | A valid replacement publishes a new worker/package generation; old-generation commands reject. | PASS |
| F1-L10 measured memory | Python lifetime peak commit plus conservative simultaneously-live native lifetime peaks stays below the policy frozen before the run. | PASS |
| F1-L11 native ownership/faults | Separate native tests cover all declared open/snapshot checkpoints; mutating/releasing the caller blob after open cannot change the native snapshot. | PASS |

Memory policy: manifest/descriptor 1 MiB, MOC 16 MiB, at most 16 atlases / 128 MiB encoded / 512 MiB decoded / side 8192; conservative Python package/decode work 768 MiB; native model 64 MiB and snapshot payload 128 MiB; combined conservative lifetime peak commit ceiling 1536 MiB. One committed worker and one replacement candidate may coexist. The logical decoder estimate includes retained old/new packages, encoded scratch, inflated rows, mutable/immutable RGBA copies and row scratch. Windows counters use [GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo) and [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex). These are measured local acceptance limits, not an OS memory sandbox or a claim of actual machine-wide OOM testing. Detailed private measurements remain ignored.

## API/window and corroborating checks

The final visible harness launch used `python scripts/moc3_test_harness.py --window --duration-seconds 35`. The loopback client created a control session and ran `F1-LOCAL-REPLAY`, observing `TRUSTED_LOCAL_F1_CPU_VERIFIED`, proof tier `trusted_local_cpu`, and the shared PACKAGE → EVALUATOR → NEGATIVE → MEMORY → COMPLETE events. A second run was cancelled and reached `CANCELLED`. The window closed normally and the token descriptor was removed. The API packet is ignored under `build/moc3-f1-local/`; its source hashes match the final harness/client. Model-derived data and the control token are not in this report.

| Command/check on the local machine | Result |
| --- | --- |
| `cmake --build build/moc3-p1-native --config Release --target ars_moc3_harness_bridge ars_moc3_snapshot_tests` | PASS / exit 0 |
| `python -m unittest discover -s tests/moc3 -p test_package_admission.py -v` | PASS / 11 tests |
| `python -m unittest discover -s tests/moc3 -p test_harness_api.py -v` | PASS / 8 tests |
| `ctest --test-dir build/moc3-t0-verify -C Debug -R 'moc3_(t0_harness_api\|f1_cpu_preflight)' --output-on-failure` | PASS / 2 tests |
| `ctest --test-dir build/moc3-p1-native -C Release -R moc3_independent_snapshot --output-on-failure` | PASS / 1 public negative test |
| `python -O scripts/moc3_local_replay.py` | Expected rejection / exit 1 before model work; cannot produce an optimized false PASS |
| Independent read-only code review | PASS after fixes F1-R1 (nested manifest base) and F1-R2 (optimized verifier); no unresolved supported P0-P2 findings |

The generated package tests cover manifest-relative ordering, every PNG filter with frozen multirow pixels, CRC/path/duplicate/hardlink failures, retained-package/decode budgets, disabled optional features and every declared parser/decode allocation checkpoint. The native tests use explicit checks in Release. No claims are made for an exhaustive malformed-MOC fuzz corpus or every possible allocator instruction.

## Remaining boundaries and handoff

Missing redistributable MOC3 input is a public-CI coverage limit, not a remaining local F1 HOLD. The local model and all derived images, hashes and measurements must remain outside the public repository.

F2 can begin from the immutable package, actual texture slots and native immutable snapshot interface. It still must prove multi-atlas residency, raw-sample/color-space rules, mask/inversion/blend semantics and clean host offscreen pixels. F3 must execute the manifest-linked Physics3 data with fixed-clock traces and host pixels. The current HTTP control session remains synthetic between suite runs; persistent host model-session step/frame routes are a later integration hookup and continue to return explicit HOLD. Public activation and real physical device-loss D06/D08 evidence are unchanged.
