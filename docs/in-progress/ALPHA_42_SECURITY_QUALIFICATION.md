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
