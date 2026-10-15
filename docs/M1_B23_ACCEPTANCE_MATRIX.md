# M1-B2/B3 acceptance proposal

State: NOT_EXECUTED. Scope and numeric policies are proposed in
`M1_B23_IMPLEMENTATION_READINESS.md`; this matrix is not a test result.
Baseline B1 is main `535663960c33c0ebe49481a8681b8625c336fafe`.

## Required build and regression gates for each source phase

| Worker | Required execution |
| --- | --- |
| Ubuntu 24.04 GCC, shared and static | Foundation + real runtime components, C11/C++20, exact 20 shared exports, two independent C consumers |
| Ubuntu 24.04 Clang, shared and static | Same; preserve Linux version-script fix without relaxing export inspection |
| Ubuntu 24.04 GCC ASan+UBSan | Instrument actual source/provenance components and native test executables; inspect __asan_ and __ubsan_ and retain raw output |
| Ubuntu 22.04 GCC TSan, separate build | Instrument and execute actual concurrent components; inspect __tsan_; no suppression/retry turning a race into PASS |
| Windows-2022 MSVC x64 shared/static | Same native behavior and consumers; bcrypt linkage and actual QPC/entropy smoke |
| Windows-2022 MSVC ASan RelWithDebInfo | Inspect ASan imports; keep historical PREP assertions enabled; retain instrumented native results |

Retain existing M0 PREP/P0/Profile B, 83 M0 + seven M1-A + six B1 contract cases,
nine governance cases and 189 foundation behavior checks. Add phase-specific
custody negatives and native suites; do not reinterpret old UNSUPPORTED fixture
assertions as live source API tests. Public source/model/character/update/render
entry points remain UNSUPPORTED and capability bits/limits zero in both phases.

All workers must use the same exact source SHA with current target ancestry.
Require actual final summaries, exit codes and external-consumer execution.
An unsupported/missing sanitizer runner is HOLD, never a successful CPU gate.
No physical GPU result can be supplied by these CPU workers.

## B2 deterministic suites and expected invariants

| Test family | Controlled schedule / negative cut | Expected result |
| --- | --- | --- |
| B2-OS | Real backend reads on multiple threads plus private injected failure, signed/zero frequency and conversion overflow | OS backend called; monotonic serialization; typed failure and sticky runtime fault; no fallback or token logging |
| B2-CLOCK | Observe 180 through source A then 170 through source B; repeat with OS failure, equal times and subsequent 190 | First CLOCK_REGRESSION/CLOCK_FAILURE; later SECURITY_CLOCK_FAULT across runtime; equal samples accepted; no refill/sequence/activation on faults |
| B2-LEDGER | Inject zero, retired-token collision, pending-token collision, partial/EINTR/EAGAIN entropy and forced ledger budget | Fresh bounded retry or HANDLE_EXHAUSTED/INTERNAL; failed output zero; no old eligibility, leaked reservation or unbounded retries |
| B2-GRANT | Canonical/mixed-case/Unicode/NUL/wildcard/duplicate/unknown catalog IDs; zero allowlist; label reuse across generations | Exact catalog binding and immutable copied scope; deny-all stays deny-all; labels/packet claims never authorize |
| B2-FRAME | Null/count/overflow/short size, 257 channels, duplicates, kind mismatch, NaN/INF, bool/trigger errors, bad quaternion and confidence | Reject before unsafe reads/queue mutation; trusted invalid non-NULL native addresses are outside ABI isolation claims |
| B2-SEQ | Two threads prepared seq 100 at highwater 99; force seq 101 before 100; accept UINT64_MAX then wrap | Exactly one duplicate winner; later lower sequence invalid; no wrap; rotate required for new sequence generation |
| B2-QUEUE | Hold queue full, exhaust preparation/append reservation, retry seq 102 after restoring capacity | QUEUE_FULL/OUT_OF_BUDGET/BUSY as documented, no highwater/quota charge; valid retry accepted exactly once |
| B2-RATE | 16 live sources at policy limits; burst and 100x attempts with fixed clock steps | Bounded queues/workspaces/bytes, exact rate accounting, no trigger overwrite; private counters show accepted/rejected totals |
| B2-ROTATE | Pause unlocked B; revoke/admin_release/close completes before C; separately expire or clock-fault during B | C cannot activate; output zero; explicit terminal state preserved; old cap unusable; reservations burned |
| B2-ROTATE-FAIL | Fail CSPRNG or ledger after A; retry ROTATE_FAILED; separately retry EXPLICIT_REVOKED/EXPIRED | Retry allowed only for still-owned failed rotation, never terminal registration; no scope widening or TTL extension |
| B2-EXHAUST | Force source/control/ticket/accepted ordinal max before revoke, expiry, rotate and close | Checked terminal HANDLE_EXHAUSTED/CLOSED, tombstone published, prior token denied, no assert/wrap/reactivation |
| B2-CLOSE | Eight admitted workers paused; H sets sticky intent; eight denied contenders repeat; release existing workers | No new admission after intent; previous append before QUIESCING; RAII readers/pins balanced, close finishes without relying on shared_mutex fairness |
| B2-RECLAIM | Destroy CPU binding while copies/ingress/admin leases remain; callback reentry/OOM; query vs two finalizers | Typed registry validation before payload, direct unpin, BUSY until drain, no orphan authority/UAF/double-free/callback under lock |

Run original RACE-E/G/N/R/T/U/W component obligations and the authority portions
of A/H/K/L/M/Q/S against production objects. For RACE-A/R, preserve original
records but apply R2 atomic append and R5 CV closing precedence: do not invent
an impossible successful queue append after QUIESCING to match old pseudocode.
The complete original pose/render/adapter oracles still need later evidence.

## B3 deterministic suites and expected invariants

| Test family | Controlled schedule / negative cut | Expected result |
| --- | --- | --- |
| B3-CLOSURE | Source values pass through blend, smoothing state, delayed trigger and composite cache; include zero weight/cancelled values | Every future-relevant effect retains transitive source epochs; bounded union rejects overflow instead of dropping dependencies |
| B3-REVOKE | Stage A+B plus retained state, pause before final gates; revoke A then resume | Candidate AND staged writes rejected; next update neutral+B only, B unchanged; pose generation does not advance on reject |
| B3-EXPIRY | A expires on ingress while S holds staged A+B; release tombstone, then S acquires it | No source->pose acquisition; stale A state retired by S; exact RACE-V A+B to B-only recomputation |
| B3-BASELINE | Revoke both A and B; mix another frame after historical snapshot | Clean authored neutral, no revoked smoother/trigger/last-pose seed reused |
| B3-CLOSE | Old S pin paused before commit; H begins close then S resumes | No new pose after QUIESCING, no blocked direct unpin or finalizer claim leak |
| B3-TIME | Pause before final sample across expiry; separate private cut after valid sample | Before-sample rejects; after-sample case records permitted sample-time behavior, not physical deadline/scrub |
| B3-CLOCK | Cross-source admission then publish sees lower sample; poison then larger clock | Sticky runtime failure prevents both new queue authorization and publish; no independent publisher watermark |
| B3-ATOMIC | Fail final authorization/allocation or exceed union/node/state/trigger/snapshot budget | No partial retained-state install, no pose increment, prior immutable snapshot remains valid historical state |
| B3-SNAPSHOT | Reader pins previous snapshot across logical binding/runtime close; exhaust pool; race reader with publication | Acquire/release immutable revision, direct lifetime pin; finish BUSY until release; no eviction/UAF/free under gates |
| B3-ORDER | Multi-source final validation, revoke, expiry, rotation and close in controlled orders | Stable source-slot lock order, no pose/source/clock/router inversion, same accepted input gives deterministic CPU result |

B3 adds component obligations for RACE-A/B/C/F/H/I/K/M/O/P/Q/S/V. It does not
claim a real mode switch, complete mixer/retarget pipeline, host adapter or GPU.
CPU lineage through a representative delayed effect is not all of SRC-024.

## Schedules, audit evidence and negative controls

- Use barriers, CV/latches and event counters; no sleeps as ordering evidence.
  Do not compile fixture counter/backend overrides or cut points into shipping
  component objects. Use test-only definitions that exercise the same algorithms.
- For every required race family, run controlled cuts and at least 1000 seeded
  iterations. Proposed fixed seed is `0x4152534232330001`, recorded with the
  iteration and event trace on failure. Cover inverse ordering, not just one winner.
- RED-first tests must fail for missing/wrong production behavior, with expected/
  actual status and exit code. A source-path failure is custody RED, not a race
  or sanitizer execution. Keep failing source SHA and raw log before GREEN.
- Evidence fields: case/component ID, PR/source/base/tree, OS/compiler/version,
  flags, seed/schedule, expected/actual status and invariants, stdout/stderr/exit,
  backend instrumentation, artifact digest and reviewer class. Never log bearer
  token values; log counters or opaque test labels only.
- Add negative custody checks for moved main, missing ancestor, extra/missing
  source/test path, frozen header/design mutation, historical B1/B2 test dilution,
  fabricated full-case PASS, public source activation and mock backend exports.
- Mutation-oriented audit must show the oracles detect omitted final epoch check,
  quota charge before append, reactivated revoked rotation, missing tombstone
  acquire and partial staged-state commit. Mutants remain in isolated test build
  artifacts, never source-authority commits or public feature toggles.

## Exit decisions

B2: B2_CPU_AUTHORITY_COMPONENT_PASS only after the complete exact-source matrix,
native backends, bounded fault/race tests and historical regressions pass.
B3: B3_CPU_PROVENANCE_COMPONENT_PASS only after B2 lands and its own full matrix
and deterministic publish/cache/close evidence pass. Same-account SELF_AUDIT /
INDEPENDENCE_NOT_AVAILABLE is disclosed; neither assessment grants merge.

The separate 87-case plan preserves all original fields and prerequisites.
Whole-case results remain HOLD until the entire original scenario actually runs
with all required model/character/adapter/mode/platform/GPU dependencies.
Five explicitly GPU-marked cases remain HDL-009/015/019 and RACE-D/J. Additional
whole-case dependencies are not removed merely because their oracle is CPU-testable
in isolation. No gate can be waived by renaming a private fixture as a public API.
