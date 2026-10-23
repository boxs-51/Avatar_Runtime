# Alpha42 stage2: public authority, replay and close

2026-10-23, Asia/Bangkok. Continuation from PR80 source
`69d90e100610ba5b605b2a05e588a46d81dd0f66` on the same stacked branch, whose
base remains PR79 `d75fd268cea9880a5224403e69c010357fbb3580`. The owner requested
remaining work with Actions temporarily excluded. The [stage2 contract](../../in-progress/ALPHA_42_SECURITY_QUALIFICATION.md)
preserves the original87-case/platform/sanitizer obligations. The earlier
[eight-case report](ALPHA_42_WINDOWS_SECURITY_QUALIFICATION.md) retains its
original bytes, source pins and historical scope.

## Direct local result

Windows10 build19045, MSVC x64, static CPU-opt-in, D3D11 OFF, no sanitizer.
Release runner PASS23 local observations,64 NOT_RUN;26 command exits0 and164
source inputs stable before/after. Original8 cases remain linked public C ABI
consumers. The15 new cases use a separate **PUBLIC_ABI_NATIVE_OBSERVER** test
executable: all model/character/source/submit/update/close mutations traverse
the public ABI on a real validated model. Existing native friend seams read
actual CPU snapshots, queues/credit/sequence/epochs and schedule admission cuts.
The security clock is deterministic injection; append failure is an existing
injection seam. These do not establish real OS clock or physical OOM failure.
No runtime source, ABI/export/header, production cap or frozen corpus is changed.

| Added case | Asserted direct observation |
| --- | --- |
| SRC-001 | Unregistered token denied; queue/sequence/quota and existing nonneutral CPU pose unchanged; next generation retains authorized pose |
| SRC-005 | Mouth-only grant denies yaw; committed mouth/yaw and counters unaffected |
| SRC-008 | Duplicate/decreasing sequence rejected without charge or pose rewind |
| SRC-009 | UINT64_MAX accepted once; zero/one wrap denied; rotation enables fresh sequence1 and old cap stays denied |
| SRC-010 | Rotation preserves diagnostic label, strictly advances authority epoch, rejects old cap and publishes fresh-cap input |
| SRC-011 | Revoked token replay at plausible new timestamp denied after reconnect; neutral fallback then fresh authorized pose commits |
| SRC-013 | NaN/INF/invalid UTF8 rejected before queue; actual quaternion semantic accepts identity, rejects zero at sequence2 without charge, accepts valid identity retry at2 and publishes w=1 |
| SRC-016 | Same label/new character generation receives distinct authority; old token denied; new character neutral until its own valid source publishes |
| SRC-018 | Explicit2D-to3D mode switch preserves semantic scope; ungranted yaw/native-bone name denied while mouth remains usable |
| SRC-021 | Mixed-case/Unicode grant aliases rejected; output handles/caps zero; issuance unchanged; canonical source still publishes |
| SRC-027 | 1000 rounds, highwater99 then two public threads at sequence100: exactly one append/ordinal/highwater advance and one quota charge; committed pose equals winning input |
| SRC-028 | Full queue rejects102 without charge; after capacity freed injected append OOM also leaves highwater/quota intact; valid102 retry succeeds once |
| SRC-029 | Deterministic thread101-before100 rejects lower sequence; one queue entry and committed winner, no ordering reversal |
| SRC-039 | One admitted ingress paused at security-clock cut; H close sets writer intent,8 later attempts denied, earlier ingress drains, close completes and post-quiesce calls denied |
| SRC-045 | Same close cut with8 already-admitted workers; writer intent rejects8 later attempts, existing readers drain and close completes within bounded waits |

All15 local executions PASS. SRC027 records each of1000 outcomes/ordinal/quota
checks in its raw log; no particular winning thread is promised. SRC029 fixes
the inter-thread order with a latch. Close cases use condition-variable
predicates for admission count and closing intent, with10-second failure bounds;
they do not use sleep to guess the cut. All fixtures require exact tracked
allocation/free identity and zero final live allocations. Native observers
protect runtime lifetime with a work pin and copy snapshot values through a
real snapshot lease; they release those pins before final close.

The cumulative same-source Debug matrix also PASS23/23,5.60 seconds,exit0.
Fresh unchanged security/component regressions PASS18/18,34.73 seconds,exit0.
Python contracts PASS189,116.155 seconds,exit0, including the new real generator
test that validates the pinned derivative and rejects same-size base tampering
while preserving the previously generated output. Component suites do not fill
unexecuted public-case cells. PowerShell logs are UTF16; unittest's stderr wrapper
can contain NativeCommandError text despite actual exit0 and final OK.

## Typed fixture and custody

The original frozen positive-dual-full bundle has scalar mouth/yaw semantics.
The derived authority-quaternion bundle adds `extra.quat` with identity default,
copies all original asset bytes and uses deterministic JSON/ZIP metadata. Its
input must match the unchanged frozen manifest. Output is pinned at5100 bytes,
SHA256 `d7e6061db9e6d42c33d9fd0b738e192b5f45c992d9098e7e767726ac741cf1f1`.
It is generated CC0 test content, not a private model. The full corpus manifest
and generator remain unchanged. Public-only cases continue using the original
bundle; native observer cases use the typed derivative and actual public loading.

Primary local receipt:
`E:/Avatar_Runtime/build/alpha42-stage2-evidence/31afb52481ec482cb6e560fca1c1ce71/evidence.json`.
It includes all87 records, exact per-case execution kind/results, compiler/OS,
commands, raw log hashes, four executable/worker hashes before/after every case,
fixture pins and source closure. Raw evidence stays in ignored local build
storage; report/pins/runner are the versioned handoff, not uploaded artifacts.

| Artifact | SHA256 |
| --- | --- |
| Receipt | `c2ba03468c7ec730696780d64b3f2d0a5e42ec1d0e5d064dc09994fecbcc4097` |
| Source map: sorted compact JSON | `d1727455c40689d6caa68eb4070df2f05e6c5c863f711c410eb4bdb387c4d34d` |
| Release native-observer executable | `cdaa2fda140587d2f541a33a975e7f599f32c4bc175d7d36517e8b568e88721f` |
| Release linked-public executable | `6ad3676e6f7b44ebde63f7177385d29cf9bc99a0296a19e4748381cf5a95a068` |
| alpha42-stage2-debug.log | `4da57dc40256a48ef9189303873a1e002bc0c6a1cf3d6f4bd5fda91545131621` |
| alpha42-stage2-contracts.log | `7b3c7a818b75a204248ae91c8c563b65f501696a5ff42e19163d1e71c09e2959` |
| alpha42-stage2-regression.log | `64915adf5cc68933294e9c8dc5436251d182ae3d9ac52743a8e57264f5442088` |

Preflights failed compiling the new test due to a missing include and explicit
out-of-range signed-char casts under warnings-as-errors. The test was corrected;
compiler checks were not relaxed. Independent review also found a P2 oracle
gap: submitting a zero quaternion under scalar mouth.open could be rejected
only for kind mismatch. The typed fixture, matching grant, identity positive
control, unchanged counters and same-sequence retry above correct that gap.
These are test implementation corrections, not production runtime RED/GREEN.
The successful source-stable receipt excludes those failed compile preflights.

## Reproduction and remaining obligations

```powershell
python scripts/verify_public_security_cases.py --build E:/Avatar_Runtime/build/a42/s --evidence E:/Avatar_Runtime/build/alpha42-stage2-evidence --dependency-cache E:/Avatar_Runtime/build/c-deps
cmake --build E:/Avatar_Runtime/build/a42/s --config Debug --target ars_public_security_cases ars_public_authority_cases --parallel 4
ctest --test-dir E:/Avatar_Runtime/build/a42/s -C Debug -R '^alpha42_' --no-tests=error --output-on-failure
python -m unittest discover -s tests/contracts -p 'test_*.py'
```

Bounded stage2 execution PASS; overall issue42 UNVERIFIED and G07/G08 HOLD.
Each of87 complete-case qualification records remains HOLD, with all original
required windows-2022/Ubuntu GCC/Clang platform cells and sanitizer obligations
NOT_RUN.23 normal local observations, including15 instrumented cases, are not
23 fully qualified cases. Remaining64 public-case executions, remote serialized
adapter boundaries, other deterministic schedules, integrated sanitizer support,
GPU retirement and physical faults are not implied by these results. The
nonneutral CPU snapshot assertions here are not rendered-pixel/rollback evidence
for issue41. No upstream HOLD is silently closed, no merge or activation occurs.

Next: failure/expiry/clock and rotation-phase cuts through the actual public
runtime, plus an explicit integrated sanitizer/platform design. The validated
model/native observation route is reusable without adding a public debug API.

Independent reviewer `/root/f2_independent_review` performed separate read-only
source/oracle and final evidence/document reconciliation, with no edits or
native executions. It confirmed the quaternion P2 correction and verified164
stable source hashes,26 command/log receipts, four unchanged binary/worker pins,
Debug/regression/contract logs, catalog174 and exact12-file cumulative PR scope.
Final bounded review PASS, no new supported findings. Commit/PR custody is to
be recorded on issue42 when this stage is published; review is not merge approval.
