# Alpha42 stage3: fault cuts and terminal-retirement repair

2026-10-23, Asia/Bangkok. Continues PR80 source374b3a3 and its unmerged PR79
base d75fd26 under the [A42 contract](../../in-progress/ALPHA_42_SECURITY_QUALIFICATION.md).
Actions remains temporarily excluded by owner instruction. Earlier stage1/2
reports keep their original source/pins and scope; their no-production-change
statements do not describe this repair.

## Three observed defects, repaired without changing terminal statuses

`HandleRegistry::invalidate` permanently retires a UINT32_MAX generation slot
and returns HANDLE_EXHAUSTED **after** logical invalidation. Three public CPU
adapters treated that status as an early failure and skipped completed-release
bookkeeping. The registry had rejected the old handle correctly, while CPU
records still occupied capacity or entered later updates.

| Local RED | Controls and symptom before repair | GREEN requirement |
| --- | --- | --- |
| HDL-005 character | Baseline update succeeds; max-generation destroy returns HANDLE_EXHAUSTED; slot permanently retired, old handle stale, next character uses another slot; CPU live count3 vs2 and next update STALE_HANDLE | CPU live count2, next update OK, old handle still stale and retired slot never reused |
| DIAG-MODEL | Terminal release returns HANDLE_EXHAUSTED; old handle stale/slot retired, but model record count2 vs1 | Completed release clears model record/value, retains original status |
| DIAG-ADMIN | Terminal admin release returns HANDLE_EXHAUSTED; old admin stale and old bearer denied, neutral update succeeds, but source record count1 vs0 | Completed admin release clears record/cache, retains original status |

All REDs used real public APIs on generated validated models, observed process
exit1, and completed the scoped runtime's final close/zero-allocation checks
before their final failing census/update assertion. The HDL005 test is unchanged
between its RED and GREEN. The two adjacent diagnostics are extra regressions,
**not two additional canonical cases**.

The minimal `src/core/runtime_cpu.cpp` repair completes character and model
cleanup for OK or completed terminal HANDLE_EXHAUSTED. Admin control does this
only for release command2; rotate/revoke failure ownership is unchanged. Typed
return codes, ABI20/header/exports, registry generation rules and production
budgets are unchanged. No retired slot becomes reusable and no old capability
is reactivated. Logical destroy/release remains complete even when its terminal
retirement result reports exhaustion.

## Direct local fault observations

Final Release runner PASS37 local observations:8 linked-public ABI and29 native
observer cases;50 inventory cases remain NOT_RUN.164 source hashes are stable
and40 commands exit0. The stage3 additions are14 canonical observations below.

| Added case | Direct assertions |
| --- | --- |
| HDL-005 | Permanent slot retirement, stale/no-alias controls, correct CPU census and healthy update after repair |
| SRC-004 | Injected RNG failure at register/rotate: typed INTERNAL, zero outputs, no leaked registration/weak fallback; old bearer denied and neutral fallback |
| SRC-012 | Expiry with queue backlog clears pending input and commits neutral with no source provenance |
| SRC-020 | Unavailable/regressing security clock blocks new publish and stays faulted; regressing host simulation callback cannot advance pose or renew source expiry |
| SRC-022 | Retired-token collision retries to a fresh token; injected issuance limit fails closed, neither retired token revives |
| SRC-025/SRC-036 | Pause at final public-batch security sample until TTL expires: reject candidate, preserve prior generation/pose, next update neutral with no old provenance |
| SRC-026 | RNG/issuance failure after rotation retires the old token: zero output, no rollback and neutral fresh pose; retryable rotate_failed is not terminal EXPLICIT_REVOKED |
| SRC-035 | Ingress backwards sample does not consume quota/sequence and faults authorization permanently |
| SRC-037 | Valid sample before pause permits its commit after underlying time advances; next sample expires source and commits neutral, matching sample-time TTL semantics |
| SRC-038 | Accept150, expire200, regress120: terminal expired/epoch invalidation, sticky global clock fault, no new pose on old authority |
| SRC-040 | Ingress sample180 then publisher170 faults the shared watermark and blocks new publication |
| SRC-042 | Rotation paused at mint phase B until expiry: phase C refuses activation, zero output; subsequent rotate cannot renew expired source |
| SRC-044 | Injected epoch/control/ticket UINT64_MAX boundaries become EXHAUSTED, do not wrap or reuse bearer authority, and permit neutral fallback |

The final-clock pause is in the actual public update path: for this single
character, batch.count becomes nonzero only after the prepared snapshot was
transferred into PublicBatch; its next security read is the final publication
authorization sample. The S thread reads its own batch; the coordinator changes
only clock atomics and releases the bounded condition-variable gate. SRC037
captures the returned sample before the pause;025/036 capture it after. No sleep
guesses a publish cut, no private method publishes instead of the public API,
and all runtime/snapshot lifetimes remain pinned.

Entropy, clock, ledger-limit and counter conditions are deterministic native
test injections. They do not prove actual OS entropy/clock failure, physical OOM,
or heap allocation failure in the fixed-array token ledger. All87 complete-case
qualification records and required-platform/sanitizer obligations stay HOLD or
NOT_RUN. No original oracle or historical90-bundle corpus was edited.

## Source and raw evidence custody

Final primary receipt:
`E:/Avatar_Runtime/build/alpha42-stage3-evidence/05c8ffb45e124237aba9cd872e355f4e/evidence.json`.
It preserves87 records, execution tiers, commands/log hashes, two fixture pins,
four executable/worker hashes and164 source hashes before/after. Raw local
evidence remains in ignored build storage, not uploaded GitHub artifacts.

| Evidence | SHA256 |
| --- | --- |
| Final37-case receipt | `9be66b028262376f20b7ab284e8bb4ffc3fdbf1ad8d956c887eebe7ea820aa32` |
| Sorted compact source-map JSON | `1f9da2a9a47ac2b140906ac38e790aafc317d47b916539e00261100cad4bec8d` |
| Release native observer executable | `0312b2f26dad75b8145f8f12d6e64a4b132fa1e508d21d283b87443f1782bbcc` |
| alpha42-terminal-red-receipt.json | `9b092abfe3ae5675e600c8b06da5d33ad4ad675e40fe667f7ac39c9ceb032e4a` |
| alpha42-adjacent-red-receipt.json | `51130867b0c27feb27124d649f8f050c10f88cab774a79ae61c959d2a3bde0f7` |
| alpha42-stage3-contracts.log | `2968f2402bcda3733c413f36dbd2b120aed05e22969bcd5267a3f9cb7eeb601c` |

The RED receipts identify commands, exits, source/test/binary/log hashes and
healthy/stale/retirement/cleanup controls. Preliminary36-case and character-only
repair receipts retain their actual source boundary and do not qualify the final
three-fix source. Contract tests PASS189,150.320 seconds,exit0; stderr wrapper
text in the PowerShell UTF16 log is not its process exit code.

Final supplemental receipt:
`E:/Avatar_Runtime/build/alpha42-stage3-supplemental/evidence.json`, SHA256
`c0f770381d57d530a707972542d23f9180d5d0b1680f34abca02db213cbe51e4`.
It requires the same164 source hashes before each of six commands and after
completion, and records raw command/log exits/hashes plus final executable pins.
All6 commands exit0 and the receipt PASS. Its final-source results are:

| Execution | Result |
| --- | --- |
| Static Debug canonical37 plus2 extra retirement diagnostics | PASS39/39,7.41 seconds |
| Rebuilt static Release CPU/security/lifecycle/allocator/custody plus2 diagnostics | PASS25/25,38.03 seconds |
| Rebuilt D3D11 opt-in public WARP/C11/dynamic/multiscene/mip | PASS5/5,11.03 seconds |

These GPU regressions exercise healthy public rendering/drain after the repair;
they are not a forced terminal-generation GPU retirement case, physical loss,
or a new four-configuration installed-SDK qualification. Earlier installed/fuzz
receipts keep their original closure; they are not relabelled to this production
revision. Source-level default flags and ABI exports are unchanged.

## Remaining work and gate disposition

The remaining50 canonical executions are12 HDL,23 RACE and15 SRC; existing
component suites and overlapping SRC observations do not silently fill these
records. Their next entry criteria are:

| Remaining group | Required work/limit |
| --- | --- |
| HDL006/009/010/015/018/019 and SRC032 | Exact public GPU snapshot/three-character release/drain/retirement cuts on healthy WARP; this is distinct from physical loss #40 |
| HDL007/008/011/013/017 | Controlled copy/update/lookup/callback/query/finalizer schedules respecting actual thread ownership |
| HDL016 | Reconcile allocation-free registry pin/tombstone design with a meaningful fault-bearing allocation envelope; no invented OOM or silent N/A |
| SRC014/041 | Sustained16-source pressure, quotas/counters, bounded queues and close liveness |
| SRC002/003/006/007/015/031 | Actual serialized remote adapter/authentication boundary and non-disclosure/length/identity oracles; direct C ABI alone is insufficient |
| SRC023/024/043 and RACE-F/V | Complete retained smoothing/trigger/retarget lineage, beyond current public retained-input observations |
| SRC030/033/034 and RACE-K/L | Public management interleavings conflict with current single-H ownership; reconcile supported semantics before a dependent implementation/test |
| Remaining RACE IDs | Run their full named schedules and assertions explicitly; a related SRC test or old private-component aggregate is not a per-case PASS |
| Required platform/sanitizers | Actual Windows2022/Ubuntu GCC/Clang plus separately supported integrated sanitizer profile; local Windows10/no-sanitizer does not substitute |

Overall #42 UNVERIFIED, G07/G08 HOLD; issue41 Linux/pose, #40 physical runner,
#61 integrated qualification and #62 release stay separate. No main/alpha merge,
activation, private-asset redistribution or driver reset is performed. The
terminal-retirement repair is a bounded functional/security result, not a
whole-milestone or shipping certification.

Independent reviewer `/root/f2_independent_review` performed separate read-only
oracle/security/source and final evidence/document reconciliation without edits
or native workloads. It confirmed the completed-destruction semantics and all
three minimal adapter repairs, then verified primary/supplemental/current164
source equality,40+6 command/log receipts, executable/worker pins, RED/GREEN
controls, exact14-file cumulative PR scope and catalog175. Bounded review PASS,
no new supported findings; task PASS, overall #42 UNVERIFIED. Exact commit/PR
custody is recorded on issue42 when this stage is published; no merge approval
or independent execution is implied.
