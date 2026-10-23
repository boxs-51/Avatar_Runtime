# Alpha42: production security qualification

2026-10-23, Asia/Bangkok. User requested continuing other issues, with GitHub
Actions temporarily excluded because of quota. Issue42 is next runnable work.
Base `d75fd268cea9880a5224403e69c010357fbb3580` includes the unmerged Windows
issue41 slice; this is a stacked branch, not integrated alpha evidence.

| Requirement | Observation required in this slice |
| --- | --- |
| A42-1 | Preserve all87 canonical HDL19/SRC45/RACE23 IDs, scenarios, platforms and oracles from reference inventory; never edit frozen inputs |
| A42-2 | Execute HDL001/002/003/004/012/014 and SRC017/019 as separate processes against real enabled public CPU C ABI, no contract-model or private runtime access |
| A42-3 | Record exact commands, exits, source/input/binary/log hashes, seed, compiler/OS, per-case assertions and cleanup;100000 seeded character reuse cycles for HDL014 |
| A42-4 | Fail closed for missing/duplicate/unknown/mismatched result markers, source drift or case failure; add meaningful verifier negative tests |
| A42-5 | Keep every unexecuted case, required-platform and sanitizer cell NOT_RUN; all87/Windows2022/Linux/sanitizer closure and G07/G08 remain HOLD |
| A42-6 | Independent source/evidence review, focused commit/PR and documentation reconciliation; no merge, shipping activation, GPU reset or private assets |

The reference inventory/regression gate are normative case oracles with historical
design-only headers. Existing B1/B2/B3 tests are real production-component proof
but cannot be relabelled as these new linked-public executions. The ordered
backlog is a current work coordinator; its earlier integration counts are
historical. No dependency HOLD is waived: issue41 stays open, and broader42
completion still requires integrated dependencies/platform/security evidence.

Build one Windows x64 Release static CPU-opt-in profile (D3D11 OFF), generated
positive dual bundle, unchanged production runtime. Register each case in CTest
and run the same binary through an auditable per-case runner. Host allocation
callbacks assert exact address/size/alignment custody and zero final allocations.
CPU-only tests do not claim GPU retirement, physical loss, nonneutral-pose proof,
sanitizer cleanliness or remote authorization/non-disclosure proof.

No new product behavior is intended; this is qualification implementation.
Failures must be diagnosed against the original oracle, not patched by weakening
expected statuses. Normal local Windows observations supplement the inventory's
required windows-2022/MSVC, Ubuntu GCC/Clang and separate sanitizer obligations.

See the [local observation report](../completed/audits/ALPHA_42_WINDOWS_SECURITY_QUALIFICATION.md)
for exact results, failures/corrections, source/binary/log pins and outstanding
work. Its completed classification is for the bounded experiment, not87-case
qualification, issue closure or G07/G08 acceptance.

## A42 stage2 continuation

Owner requested continuing remaining problems after PR80 at69d90e1. Preserve
A42-1..6 and extend the local observations with SRC001/005/008/009/010/011/013/
016/018/021/027/028/029/039/045. A separate native test executable performs real
public ABI operations on the validated bundle and uses existing test-only friend
seams to observe committed CPU snapshots, queue/credit/sequence/epochs and to
schedule the security clock/admission cut. This is PUBLIC_ABI_NATIVE_OBSERVER,
distinct from the original header-only linked consumer. No new runtime exports,
production logic or synthetic catalog/model is introduced. Injected deterministic
clock/append failure is not real OS-clock/allocator-failure proof. Stage2 records
both8 original public-only cases and15 observer cases; original required-platform
and sanitizer obligations remain unchanged and NOT_RUN. Source-stable Release,
Debug link/execution, existing regressions and independent review are required.
The [stage2 evidence](../completed/audits/ALPHA_42_AUTHORITY_STAGE2_QUALIFICATION.md)
records23 cumulative local observations, not whole-case or gate closure.

## A42 stage3 continuation

Owner requested continuing repository issues after374b3a3. Extend the existing
native-observer tier with SRC004/012/020/022/025/026/035/036/037/038/040/042/044.
Use deterministic entropy/clock, bounded ledger/counter injection and a pause at
the actual public CPU batch's final security sample; preserve public mutations,
real validated models, all earlier cases and full source/binary custody.
Observe snapshot generation/provenance and source terminal state, not only
return codes. Release/Debug, existing regression, negative verifier tests and
independent review remain required. No security backend or production cap is
changed. These injected observations are not physical RNG/clock/OOM proof.
Concurrent management cuts that public single-H ownership rejects remain a
specific compatibility/proof gap; no private management mutation substitutes
for the requested public interleaving. No gate or issue closes by count alone.

HDL005 fault-matrix extension: inject the next free registry slot generation at
UINT32_MAX, create a real character, destroy via public ABI, require permanent
slot retirement/old-handle denial/new-slot nonaliasing and unaffected other
characters/update. Baseline374b3a3 production source reproduced live CPU ghost
record3 vs expected2 and update STALE_HANDLE after terminal destruction; ordinary
update/stale-handle/retirement/close controls passed. Fix must finish CPU
bookkeeping on completed terminal retirement while preserving HANDLE_EXHAUSTED.
The unchanged RED oracle must pass after correction with relevant lifecycle,
allocator, public GPU/default profile and security regressions. No status/ABI
waiver or retired-slot reuse is allowed. RED evidence is pinned locally before
the production edit. This necessary repair is within continuing issue42 scope.

Adjacent-case extension: standalone DIAG-MODEL/DIAG-ADMIN reproduced terminal
model record2 vs expected1 and source record1 vs expected0 despite successful
logical release/old-handle denial/final cleanup. Treat these as extra regressions,
not two additional inventory cases. Complete the same CPU bookkeeping for model
release and admin-release terminal retirement; keep rotate/revoke error ownership
unchanged. Each RED diagnostic and its test/source/binary/log pins is retained
before those production edits. No other mutation is implied by this finding.
See [stage3 evidence](../completed/audits/ALPHA_42_FAULT_STAGE3_QUALIFICATION.md)
for the three RED/GREEN repairs,37 local observations and remaining-case entry
criteria; whole87-case qualification and release remain separate.

## A42 stage4 continuation

Continue A42-1..6 at35e9734 with sustained public ingress during close (SRC041).
Eight workers alternate all16 sources, saturate queues before the real admission
cut, continue attempts across writer intent, and drain before final close.
Require exact accepted/denied accounting, bounded completion and allocator-zero
cleanup; no private method substitutes for public begin_close or submit.
Native observers inspect only safe cuts and counters owned by the harness.
SRC014 also gets a separate pressure diagnostic:100x the configured rate for
each of16 sources, repeated refill rounds and queue-capacity/charge assertions.
Its original "drops visible in counters" oracle has no current runtime counter
API/storage: test-owned totals cannot discharge that portion, so SRC014 remains
NOT_RUN in the canonical runner. Do not add a shipping API to manufacture proof.
Release source-pinned receipt, Debug, affected regression, verifier negatives,
independent review and documentation/custody checks precede commit/push.
No production change is planned; failures require a new attributable RED before
any repair. All prior platform/sanitizer/gate/HOLD restrictions stay intact.
