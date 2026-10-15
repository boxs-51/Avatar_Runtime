# M1-B preparation: runtime lifetime and source authority

State: PREPARED_FOR_REVIEW / IMPLEMENTATION_NOT_STARTED. The owner's request is
preparation, not M1-B kickoff, a merge decision, or activation of later stages.
No runtime, security policy, public header or frozen inventory is changed here.

## Verified baseline and contract precedence

Live main: `ab21908310d48466b27bb06c92ea1623353c79f5`, the owner-approved
MERGE_COMMIT for [PR19](https://github.com/boxs-51/Avatar_Runtime/pull/19).
Reviewed M1-A source: `b1d634036e538f0866c3be77b85d2de890b85c81` (same tree).
Header blob: `1c61da4a48c9faf2001fc98ad4c084413781e61c`.
Security inventory blob: `ec026c8ab872c71791df39353a26ba3dc84eed09`.
All 9 predecessor + 8 G00 blobs remain frozen design references.

M1-A delivered build/ABI foundation, not functioning runtime/source grants:
189 behavior checks, 90 contract tests, nine governance tests; 14 PR jobs and
seven push jobs passed on its exact source. GPU and 87 runtime security-case
verdicts remain HOLD. PR18 is an approved, unmerged M1-A planning snapshot;
this preparation does not silently merge or edit it.

Read contracts in precedence order R5 > R4 > R3 > R2 > original v0.1, only for
the subjects each amendment overrides. Use the accepted ABI20 header, not the
historical `_p0` declarations. Historical PROPOSED/HOLD labels are source
snapshots; owner-approved G00 and subsequent merges are recorded in PR17/19.
They do not constitute an M1-B security implementation verdict.

## Dependency findings that bound implementation

1. **No legitimate public character yet.** M1-A model load/character creation
   return UNSUPPORTED; source registration requires a live character binding.
   Do not add a public dummy-model loader or bypass registry validation to make
   security tests run. Production CPU components can be tested through private
   fixtures that seed validated CPU objects in the actual registry. Fixtures
   must not become public exports, a second reference implementation, or evidence
   of public model/character/motion integration.
2. **Some original cases require other stages.** Five inventory cases explicitly
   require Windows D3D11 hardware; additional cases include serialized-adapter,
   real mode-switch or visual rendering behavior. A CPU trace is useful partial
   evidence but cannot satisfy those complete oracles. The full 87-case mapping
   is `docs/M1_B_SECURITY_CASE_PLAN.json`, with every case still NOT_EXECUTED.
3. **M1-A checks currently bind HEAD.** `test_m1_a_custody.py` expects exactly
   14 M1-A paths at current HEAD. Keep those historical cases strict against
   landed M1-A `ab21908310d48466b27bb06c92ea1623353c79f5`, including mock lookup
   targets. Give current M1-B an independent exact branch/base/path gate. Do not
   grow M1-A's allowlist or remove its seven tests.
4. **Foundation assertions cannot assert live-runtime behavior.** Its 189-case
   suite deliberately expects valid create/close calls to be UNSUPPORTED. Keep
   the foundation target and suite as pre-runtime regression fixtures, and test
   live-runtime behavior against a distinct runtime-core target. An old status
   expectation must not be weakened without adding the new behavioral oracle.

## Proposed stages and acceptance boundaries

Only **B1** is proposed for the first kickoff. B2/B3 below are later design
boundaries, not implicit implementation claims or auto-authorized child PRs.

| Stage | Bounded work | Exit / remaining dependency |
| --- | --- | --- |
| B1 registry/lifecycle | Nonreused runtime UIDs, typed slot/generation registry, direct RAII operation/management leases, CPU-only runtime create/caps/begin/query/finish, exclusive finalizer and bounded allocation failure | Real public runtime lifecycle + production registry component tests; no character/source grants/GPU |
| B2 source authority | OS security clock and entropy backends, bounded lifetime token ledger, immutable channel scope, sequence/quota/queue admission, sticky close intent, reservation-based rotation/revoke/expiry | Deterministic production CPU-kernel traces and native OS tests; public source APIs remain disabled pending legitimate public character path and security integration acceptance |
| B3 provenance/publish | CPU immutable snapshots and bounded transitive lineage, staged retained state, release/acquire tombstones, final commit validation and recomputation | A+B RACE-V and source/close/clock races on production components; whole renderer/adapter cases remain cross-stage HOLD |

The later source-grant activation gate needs a separate, real validated
model-to-character integration (normally coordinated with M1-C), not a test
constructor exported through ABI20. Actual visual/mode behavior may require M2.
Serialized motion authentication/decoding is a trusted host-adapter responsibility
outside native asset parsing; it is not automatically delivered by M1-C.
GPU retirement evidence is M1-D. No batch/Wave or parallel-agent execution is
authorized; retain parent-first, exact-source decisions for subsequent work.

## First kickoff proposal: B1 exact source ownership

`docs/M1_B_IMPLEMENTATION_MANIFEST.json` lists 24 exact proposed B1 paths and later
component topics. The first implementation branch is `m1/b1-runtime-lifecycle`,
from actual current main at kickoff. If this documentation PR lands first,
refresh the target main pin transparently before coding. Do not retain an old
base merely because a preparation file mentions it.

Build choice: keep `ars_foundation`/`ARS::Foundation` and its existing tests.
Add opt-in `ARS_BUILD_RUNTIME_CORE=ON` for `ars_runtime_core`/`ARS::RuntimeCore`,
sharing the accepted ABI version and bounded argument validation. A private
compile definition selects the unsupported functions not yet implemented by
the core; the public header stays unchanged. A consumer must link exactly one
of these libraries. Never create two registries for one runtime or allow the
PREP stub to satisfy a production-core consumer's symbols.

Core-only functions becoming implemented in B1:
`ars_runtime_create`, `ars_runtime_get_capabilities`, `ars_runtime_begin_close`,
`ars_runtime_query_close`, `ars_runtime_finish_close`. ABI version remains real.
`drain_gpu` and all model/character/source/update/render APIs remain explicitly
UNSUPPORTED; the runtime has no GPU resources or public feature bits. Do not
advertise native-2D, GLB, HOST_DEVICE, dual-mode or motion-ingress capabilities.

Registry data is private. A public handle is checked runtime UID + expected
type + slot bounds + generation + state, never cast to an address. Acquire
runtime/object pins before releasing registry locks; unpin directly through a
stable control block even after logical invalidation. Slot generations/UIDs
never wrap; exhausted slots retire permanently. Destroy/close invalidates
logical access before physical reclamation. No external callbacks under locks.

### B1 decisions proposed for owner acceptance

| Decision | Recommended behavior |
| --- | --- |
| Allocation | Both allocator callbacks NULL uses aligned SDK allocation; exactly one supplied is INVALID_ARGUMENT. A host pair is retained until final reclamation; record byte/alignment ownership and invoke outside internal locks. Creation failure publishes no handle and returns typed OUT_OF_BUDGET/INTERNAL as applicable |
| Bounds | Internal reviewed policy defaults: max 64 live runtimes/process, 1,024 registry slots/runtime, 1,024 simultaneous operation leases/runtime. No new public flags; forced lower limits only in private test injection. Checked exhaustion returns existing typed status and cannot silently evict live entries |
| Error precedence | Preserve M1-A's invalid-buffer no-write and fixed-output-zero rules. Validate C argument shape before runtime lookup; zero runtime ID is INVALID_ARGUMENT, unknown/stale ID is STALE_HANDLE, closed work admission is CLOSED. Runtime/slot capacity or allocation failure is OUT_OF_BUDGET, transient pin-capacity contention is BUSY, identity counter exhaustion is HANDLE_EXHAUSTED. finish before begin is BUSY with no side effects |
| UID lifetime | Monotonic nonzero UID allocator, nonreuse for process lifetime, checked UINT64 exhaustion; a failed creation may burn an ID. No claim that native handles are persistent cross-process identities |
| Thread ownership | Successful create records trusted control thread; begin-close enforces it. query/finish are thread-safe management exceptions, preserving the real competing-finalizer oracle in HDL-017. CPU host allocator callbacks must support the calling finalizer thread; GPU drain remains R-only and unsupported here |
| Close intent | B1 closes work-pin admission permanently while existing CPU leases retire asynchronously; it does not wait for all CPU pins in begin-close. B2 additionally drains already admitted ingress before QUIESCING under R5. No wall-clock liveness guarantee for a stalled trusted thread; BUSY retains safe ownership and sticky intent |
| Close results | First successful begin returns OK, repeated begin returns CLOSED. query is allowed during closing; finish returns BUSY while any other CPU/management pin exists, releases an unsuccessful finalizer claim, and atomically unlinks only when safe. After success, new lookups return STALE_HANDLE |
| CPU-only completion | Zero GPU resources is an actual B1 invariant. Finish need not invoke unsupported drain_gpu when no GPU work was ever created; a test counter is not physical GPU completion evidence |
| Public close counters | Counts include the query's own management lease and all other tracked categories. FENCE_COMPLETE describes drained work/GPU after close intent; it does not promise finish succeeds while query/another management lease is active. All counters remain auditable; no fabricated zero count to make finalization pass |
| Capabilities | For a valid live CPU runtime, return OK with zero feature bits and zero unimplemented feature limits, using the existing bounded output contract. On stale runtime or bad buffer return existing typed errors; no new capability/status/ABI symbol |

The table supplies proposed details, not a unilateral amendment to ABI/security
contracts. At kickoff resolve any conflict against the accepted clauses before
implementation. Public arbitrary native pointer sandboxing remains out of scope.
Host callbacks must obey the C ABI; prevent SDK exceptions escaping C, but do
not claim isolation from arbitrary trusted-host code or OS scheduling stalls.

## R5 concurrency and later authority invariants

- Lookup: router -> handle table only long enough to validate and pin; release
  before commit/pose/source gates. No callbacks, blocking I/O or GPU calls there.
- Admission barrier: mutex + CV + bounded admitted count. Its mutex never nests
  with source/clock/pose/router locks; RAII releases on every return. Close sets
  intent before waiting, releases the admission mutex during CV wait, and does
  not rely on `shared_mutex` reader/writer fairness.
- Publication/rotation activation: commit -> character pose -> sources in stable
  slot order -> security clock. Expensive copy/validation/mixing/allocation and
  CSPRNG issuance happen outside those locks, then authorization is rechecked.
- Explicit revoke: pose -> source, no reverse acquisition of commit. Ingress
  expiry: source -> clock only; release-publish tombstones, never take pose or
  mutate simulation-owned caches. S acquire-observes and retires staged lineage.
- Security clock: OS-owned, serialized runtime high-water, sticky poison on
  regression/failure; host simulation/capture time never extends leases. TTL
  uses checked addition and exclusive `now < expiry`, with sample-time semantics.
- Rotation A invalidates old authority and reserves epoch/control/ticket; B mints
  OS CSPRNG tokens and ledger reservations outside locks; C checks every captured
  identity, runtime state, clock/expiry and reservation before activation. Revoke,
  expiry, close and exhaustion dominate; no rollback to old authority.
- Capability lifetime nonreuse applies within the runtime, as the original
  contract states. The ledger is bounded; collisions retry within a bounded
  budget, then fail closed. Do not replace CSPRNG with a PRNG or promise a new
  cross-process/persistent-token guarantee. Never log actual bearer values.

Before B2 kickoff define exact policy limits and canonical-ID grammar from the
accepted semantic contract. Proposed native backends are Windows QPC plus
BCryptGenRandom and Linux clock_gettime(CLOCK_MONOTONIC) plus getrandom, with
overflow/failure/partial-read handling and private deterministic fault injection.
Production has no public mock clock/RNG, security reset, unsigned fixture token
or fail-open fallback. Public feature activation needs separate native evidence.

## Test and CI plan

B1 first writes failing real-core tests: runtime create/close/stale lookup,
generation/UID exhaustion, type/slot mismatch, balanced direct leases, allocation
failure, reentrant allocator outside locks, query versus finalizer, wrong thread,
and 100,000 seeded registry cycles with bounded metadata. UID-exhaustion and
nonreuse tests use isolated subprocesses/router fixtures; never reset a live
process-global authority counter to make subsequent tests run. Private test hooks may
control counters, allocation and interleavings; they compile only into tests and
must not enter exports. They exercise the production components, not a copied
contract-model algorithm. Deterministic barriers/CVs/latches, never sleeps as
ordering evidence. Report BUSY/WRONG_THREAD interleavings honestly.

Later B2/B3 traces include duplicate seq exactly once, full-queue/OOM no quota or
sequence charge, terminal explicit revoke during rotation B, sticky clock fault,
expiry before/after final sample, eight admitted workers plus denied ingress
flood (RACE-T), staged A+B versus A expiry (RACE-V), and checked terminal overflow
(RACE-W). Add at least 1,000 seeded stress iterations per required race in
addition to controlled traces, preserving seed and event ordering.

Acceptance matrix: Linux GCC/Clang and Windows MSVC x64 shared/static; Linux
ASan/UBSan and **separate** TSan builds; Windows ASan in a supported configuration
with raw evidence, otherwise explicit HOLD. Verify sanitizer instrumentation
and retain logs, not just configuration labels. A runner limitation is a missing
gate, not a successful race test. Exact same source across all required workers.

Maintain M0's 83 cases, M1-A's seven historical custody cases, nine governance
cases, 189 foundation behavior checks, C11/C++20 layouts/signatures, exact 20
exports, PREP native tests and independent consumers. Add separate core consumer,
B1 custody negatives and real lifecycle tests. Existing M1 workflow and new
M1-B workflow must run the appropriate target suites without assuming source
grants exist or reusing foundation UNSUPPORTED assertions against live runtime.

Every security verdict needs ID, exact SHA, OS/compiler/flags, deterministic
schedule/seed, expected/actual status and invariants, stdout/stderr/exit code,
sanitizer evidence and reviewer class. Private fixture evidence is explicitly
component-level; whole 87-case results stay HOLD until their original scenario
and external dependencies actually execute. Do not alter the frozen inventory
to erase a missing GPU/adapter/model gate or promote a CPU partial result to PASS.

## Preparation and next authorization

This proposal adds only this document, a proposed phase/source manifest, and an
87-row case plan. All copied original scenario/oracle/platform fields must match
the frozen inventory exactly. Every row remains NOT_EXECUTED; no native test or
M1-B security PASS is asserted by documentation checks. Current-HEAD M1-A custody
tests are expected to reject added prep paths until the separately reviewed
historical-snapshot transition; no tests are skipped or edited here.

Recommended next decision: approve **B1 only**, its exact path manifest, and the
decision table above. B2/B3 require their own exact manifests, policy limits and
activation decisions; they are not bundled into a generic M1-B YES. Each future
implementation merge needs its own exact-PR MERGE decision. Keep source history
and approved snapshots intact; no force-push, rule bypass or implicit PR18 merge.

Rollback for this preparation is close/revert its documentation proposal. Future
B1 rollback is a normal reviewed revert, preserving M1-A and the frozen ABI/R5
references. No autonomous execution between user-initiated sessions.
