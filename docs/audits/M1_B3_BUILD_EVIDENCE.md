# M1-B3 CPU provenance and P2 remediation

Owner authorized B3 implementation and both P2 fixes in the current conversation.
Actual parent is landed B2/PR23 `c76958e8398e5eefde9b77d095a320cda4aac51a`.
The 31-path B3 proposal from PR22 preparation
`c56a9cd2d04dbab08aa5cc4eedf04ab4bd7efdf5` is rebound to this parent.
No public ABI, public model/character/source/update/render activation, complete
motion/retarget/mode/adapter or GPU implementation is authorized by this work.
Exact final source and CI are attested in the implementation PR, avoiding a
self-referential source SHA here. Merge requires a separate exact-head ACK.
Classification: SELF_AUDIT / INDEPENDENCE_NOT_AVAILABLE.

## RED-first and production boundary

Native P2 RED source `2cf345a23b13b4305d9258eeeace4fd03dc74905` invokes actual
runtime callbacks: nested rotate expected BUSY, actual OK; simulation time
200 -> 100 expected CLOCK_REGRESSION, actual OK. Each executable exits 1.
Raw evidence: `build/integration-evidence/b3-p2-*-red.log`.
Native B3 stage RED source `6c328a869acdfa78656dba89c76acc27b287eead` creates
the real runtime/binding and expects private CPU preparation OK, receives
UNSUPPORTED, exit 1 (`build/integration-evidence/b3-cpu-red.log`). These are
compiled production components, not source-path custody failures or ABI mocks.

The real runtime lazily host-allocates one PublishGate context, at most 4 MiB
in addition to B2's 8 MiB. A router reservation prevents concurrent duplicate
context allocations. Allocation, alignment checks, cleanup and host callbacks
occur outside authority gates. Fixed pools bound eight in-flight candidates
and 64 snapshots, including all retained historical readers. No pinned reader
is evicted. Each independently live stage/snapshot holds a runtime root lease;
snapshot roots are included in outstanding_pins and the existing snapshot pin
category, with zero GPU retires. Internal current snapshots are detached on
destroy/close and their source/character pins released outside final gates.

Typed scalar input/add/scale/retained-cell/store/deferred-trigger/output DAG
operators derive transitive masks over full character/source generations and
source epochs from actual dequeued owned frames and retained state. Callers do
not supply optional provenance lists. Zero-weight/cancelled contributions retain
lineage. Bounds: 16 provenance keys, 512 nodes, 256 cells, 32 deferred triggers;
capacity/finite/type failures reject instead of truncating effects. This is a
representative CPU scalar/delay/trigger harness, not a complete motion engine.
Preparation copies a bounded queue view into tracked candidate scratch and
previews the head without consuming live input. Final commit revalidates every
preview's source epoch and accepted head ordinal under the complete stable
source lock set, then consumes input and swaps pose/state in the same transaction.
Graph/budget failures, stale revisions and competing consumers preserve input
for retry. No accepted ingress sequence or quota is rewound or charged again.

Preparation acquires an immutable previous state revision, never its last pose
as an unchecked seed. Acquire tombstone observation and current source checks
purge invalid retained cells, aggregate caches and delayed triggers; recomputation
starts from authored neutral plus valid retained/input values. Final commit locks
runtime commit -> one character pose -> contributor sources in stable slot order
-> B2's single security-clock gate. One final sample validates all generations,
epochs, states and expiries. One immutable pointer exchange installs pose AND
persistent state, guarded by an expected revision. Failed candidates install
neither and never advance pose generation. Overflow is terminal. Historical
readers can still inspect their immutable old revision after logical invalidation.

## Both P2 fixes

One runtime-specific atomic admission word admits shared mutator leases or one
exclusive callback, never both. Operation leases cover the actual mutation and
unlocked issuance phases, rather than checking a boolean only at root lookup.
Ingress enters mutation admission after callback/copy preparation. Management query stays
available. A callback on another runtime is independent; simultaneous same-runtime
callbacks return BUSY without invoking the callback twice. Callbacks run outside
authority locks. RAII clears intent after success, exception or clock rejection.
Root leases outlive operation-admission cleanup even on failed staging/commit,
so finalization cannot reclaim the callback gate during its RAII release.
Synchronized simulation sample high-water rejects a decrease with CLOCK_REGRESSION;
equal samples and retry at the same ingress sequence remain valid. Rejection
does not charge quota or append. Simulation fault does not poison the independent
OS authorization clock and never authorizes TTL or rate.

## Verification and remaining holds

Native suites: b3_provenance (closure and P2), b3_snapshots (budgets/lifetime/
atomic errors), b3_publish_races (controlled epoch/state/trigger/revoke/expiry/
rotation/close/sample-time/clock/revision/reader/order cuts). Race seed is
`0x4152534232330001`, 1000 iterations per required grouped family, barriers/CVs
instead of sleeps. Local MSVC shared passed 19/19 groups, static 17/17 and
ASAN 17/17, with actual ASAN imports inspected on ars_publish_race_tests.
Initial exact-source CI at aa3b2903bc677027c280c58d1f24a93e61c4a7f9 passed
21/21 jobs, including all nine B3 native/sanitizer workers and both external
C consumers per worker. Additional callback-entry, source-generation and host
allocator fault cases are included in the final source; final exact-source CI
and review disposition are attested in PR24. A matrix declaration or earlier
source's success does not certify the final source.

The existing callback-paused B2 A/H trace now requires BUSY for concurrent
revoke while the callback is active, as mandated by the P2 fix; after callback
completion revoke succeeds and further ingress is denied without charge.
All other B2 traces remain. The B3 staged A/H cut separately proves that revoke
before final pose authorization rejects both the candidate and staged writes.
Historical B2 custody remains tested at its exact landed snapshot, including
dynamic ABI mutation targets; old gates and all original inventory records stay
unchanged.

Isolated native mutants cover omitting
final epoch/tombstone binding; omitting staged tombstone reconciliation/purge;
writing persistent state before authorization; charging quota before append;
and permitting phase C to reactivate a terminally revoked source. Raw logs and
results.json: `build/b3-mutants`. The tombstone mutant tests missing semantic
consumption; x64 tests alone do not prove acquire-vs-relaxed ordering on every
weak-memory architecture. Production acquire/release ordering is also inspected
in the source audit, without claiming a physical weak-memory platform run.

Post-ready self-audit found destructive input consumption during preparation;
the final implementation uses tracked immutable queue views, validates head
ordinals and consumes inputs only in successful pose/state commit. Native tests
cover graph failure, competing stage revisions and another real consumer, with
no lost accepted frame. Automated review additionally identified the check-only
callback/mutator admission race; the shared/exclusive atomic admission word fixes
that window, with 1000 controlled mutator-before-callback native iterations and
the inverse callback-before-mutator trace. Earlier source CI is historical only;
the repaired final source must pass its own complete matrix.

Allocator fault coverage includes NULL, exception, misalignment, close reentry,
recursive context preparation, retry, matching sizes/alignment and all final
frees outside gates. Snapshot pool/revision/pose overflow rejects preserve the
old immutable revision. Candidate storage and default memory construction are
bounded; local builds use two compiler workers after an unrestricted MSBuild
attempt exhausted this host's compiler memory. This build-host limit was not a
runtime fault and no test iteration or assertion was reduced to hide it.

Retain historical B1/B2 contracts and snapshot-bound custody negatives, frozen
ABI20/export oracle, inventory and 9+8 design blobs. All 87 original whole-case
verdicts remain HOLD, PASS count zero. CPU component results do not substitute
for validated public integration, complete motion/adapter/mode or physical GPU
evidence. Time semantics are final-sample authorization, not wall-clock erasure.
Rollback after a separately approved merge is a reviewed revert preserving B2.
