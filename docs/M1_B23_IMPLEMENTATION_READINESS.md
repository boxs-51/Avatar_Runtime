# M1-B2/B3 implementation preparation

State: PREPARATION_ONLY / IMPLEMENTATION_NOT_AUTHORIZED / MERGE_NOT_AUTHORIZED.
The owner requested preparation of B2/B3. This proposal changes documentation
only. It does not activate public source APIs, B2/B3 code, model loading or GPU.

## Verified baseline and reuse

Main is `535663960c33c0ebe49481a8681b8625c336fafe`, the owner-approved
MERGE_COMMIT of PR21 source `904c4c77b96199aa9360e6fa3c1ad5cfaf601c50`.
That source passed 23 CI jobs, including GCC/Clang/MSVC shared/static,
Linux ASan/UBSan and separate TSan, Windows ASan, 96 contract and nine governance
tests. This is historical B1 evidence, not B2/B3 execution evidence.

Reuse `ARS::RuntimeCore`, the actual typed HandleRegistry, direct RuntimeLease,
nonreused UIDs, terminal slot retirement, bounded host allocation and exclusive
finalization. Keep `ARS::Foundation` with its original 189 behavior checks,
the frozen ABI20 header and exact export checker. B1's bool close state and two
lease categories are insufficient for R5 ingress drainage and CPU snapshot pins.
There is no security clock, entropy backend, token ledger, source authority,
owned ingress queue, validated semantic catalog or pose publication state yet.

Header blob: `1c61da4a48c9faf2001fc98ad4c084413781e61c`.
Inventory blob: `ec026c8ab872c71791df39353a26ba3dc84eed09`.
The 9 predecessor and 8 G00 design blobs remain immutable.
PR18 and PR20 are unmerged historical planning snapshots; they are not source
predecessors to merge implicitly. PR21 is the actual landed implementation.

Normative precedence is R5 > R4 > R3 > R2 > original v0.1 on amended subjects.
Historical PROPOSED/HOLD headings do not override the accepted design and owner
merge decisions. Exact clauses: R5 sections 1-3, R4 CLK-01..05/CLS-01..02,
R3 sections 1-4, R2 sections 2-7 and the original source contract's ASCII grammar.

## Sequential rollout and explicit boundaries

| Phase | Result | Still excluded |
| --- | --- | --- |
| B2 | Production CPU source-authority kernel attached to the real runtime: OS time/entropy, immutable grants, lifetime issuance ledger, bounded owned ingress, R5 closing barrier, rotation/revoke/expiry | Public source grants, public character constructors, pose/motion/render implementation |
| B3, after B2 lands | Bounded production CPU provenance and snapshot commit components using B2's authority: staged retained state, dirty tombstones, final authorization and neutral+B recomputation | Full retarget/motion pipeline, real mode switch/adapter, public update/render, GPU completion |
| Later integration gate | Separate validated public model-to-character path, coordinated with M1-C; separately reviewed authority/ABI integration and evidence | No implicit activation merely because private components pass |

Recommended next approval is **B2 only**, its exact manifest and decisions below.
B3 scope is prepared here, but its implementation kickoff requires another owner
decision after B2's separately approved merge. No Wave, batch merge, parallel
agents or autonomous execution between user sessions. Each source PR needs its
own exact-head MERGE_COMMIT approval under ARS-M5.

B2 starts directly from actual main after approval. If this preparation lands
first or main moves, refresh the base transparently before writing code. B3 has
no invented parent SHA: bind it to the actual landed B2 merge, then revalidate
its exact paths and gates. Do not branch B3 from old B1 or merely a doc proposal.

## Decisions proposed for B2 approval

These are internal bounded policies, not modifications to ABI20 or R5. Numerical
limits are proposed; current public capabilities stay zero. All arithmetic is
checked before multiplication/addition and there are no public override flags.

| Item | Proposed policy |
| --- | --- |
| Bounds inherited | 64 runtimes, 1024 total registry slots and operation pins per runtime; B2 uses the same table and lifetime authority, not a second router |
| Source bounds | 16 active/reserved authorizable sources per runtime; at most 64 source records including terminal records with retained admin/pins; admission capacity is released only after terminal invalidation, never by evicting a live record |
| Character bindings | At most 16 private CPU character-authority records/runtime, each with an immutable validated semantic catalog of at most 256 channels; no model/character C entry point becomes implemented |
| Semantic IDs | Exactly ASCII `[a-z][a-z0-9_.-]{0,127}`, bytewise case-sensitive. No Unicode aliases, embedded NUL, trimming, case folding or wildcard. This grammar already exists in the frozen source contract section 3 |
| Descriptor ownership | Deep-copy immutable catalog/grant IDs and label before registration returns; label 0..256 UTF-8 bytes without NUL, never authority. Allowlist max 256 distinct catalog IDs; empty grant is deny-all. Native bone/morph IDs do not bypass semantic binding |
| Grant values | Fixed priority retains the full int32 range; per-frame channel max 1..256 and no larger than applicable nonempty scope. TTL 1..3,600,000,000,000 ns, checked-add at registration; rotate never extends original expiry. Expanded scope requires new trusted registration |
| Frame validation | Nonempty, max 256 channels; counts/pointers/known-size/version/flags before reads; canonical distinct IDs, kind matches validated catalog; finite values and confidence in [0,1]. Bool/trigger value 0 or 1; quaternion squared norm within 0.001 of 1, never silently normalized. Finite unused value components confer no extra channels |
| Owned input | At most 48 KiB total bounded frame+ID input copy, with at most 16 concurrent preparation workspaces. Copy/callbacks outside authority locks; unavailable workspace returns BUSY. Compact immutable queue records use catalog indices and pinned immutable grants, max 8 KiB/frame |
| Admission | At most eight admitted ingress workers/runtime; OPEN gate at capacity returns BUSY, CLOSE_REQUESTED returns CLOSED. The 16 preparation workspaces do not imply 16 concurrent admissions |
| Queue | 16 frames/source, no implicit latest-wins overwrite. Triggers and continuous frames both receive QUEUE_FULL on full capacity; no silent trigger loss. All queue storage/reservations exist before the short admission commit |
| Quota | max_frames_per_second 1..240; integer token bucket with one-second burst equal to the registered rate, initially full. Credit/refill computation is staged and overflow-safe; only successful append charges one frame and advances sequence/accepted ordinal |
| Allocation | B2-owned allocations charged against 8 MiB/runtime, including queues/catalogs/ledger/scratch/metadata. Limits need not be simultaneously achievable under byte budget: fail OUT_OF_BUDGET before state mutation. Allocate or reserve outside locks through the retained host pair; no eager allocation of every maximum-sized source queue on bare runtime create |
| Issuance | 4096 unique nonzero 128-bit token ledger entries per runtime lifetime, including abandoned/retired reservations; at most eight mint attempts per issuance. Slots/tokens are never recycled. Ledger full or collision budget exhausted => HANDLE_EXHAUSTED; OS entropy failure => INTERNAL, no weak fallback |
| Entropy retries | Windows one system-preferred BCryptGenRandom call per attempt. Linux getrandom(GRND_NONBLOCK), at most eight syscalls to assemble a token, handling partial reads/EINTR; EAGAIN/other failure/zero read/budget exhaustion => typed failure. Wipe temporary output on every failure |
| Clock errors | First observed backwards authorization sample => CLOCK_REGRESSION; first OS/conversion failure => CLOCK_FAILURE; set sticky runtime fault before returning. Later authorization => SECURITY_CLOCK_FAULT. Never reset the high-water/fault or use simulation/frame time for TTL/rate |
| Suspend semantics | TTL is measured in the selected OS monotonic domain. Linux CLOCK_MONOTONIC excludes suspended time; this component phase makes no suspend-inclusive wall-clock expiry promise. A stricter cross-platform suspend/remote session policy remains a separate public-activation decision |
| Error ordering | C argument shape first; invalid runtime ID INVALID_ARGUMENT, stale runtime STALE_HANDLE; closing denies work CLOSED. For ingress, unknown/retired cap is SCOPE_DENIED; a resolved current source then checks sticky clock/terminal state, scope, sequence, quota and queue without leaking bearer values. Source-admin calls preserve EXPLICIT_REVOKED/EXPIRED/EXHAUSTED/CLOSED rather than reactivating them |

Typed internal status decisions must be documented in tests before implementation.
The native component status is not a remote authentication response; a trusted
adapter is responsible for remote error normalization. Empty scopes never become
wildcard grants. Security-clock observation and terminal invalidation may advance
on rejected input; rejection must not charge quota or consume sequence.
Unknown canonical IDs in a registration's catalog binding are INVALID_ARGUMENT;
out-of-grant frame IDs are SCOPE_DENIED. Optional-channel behavior in a later
motion pipeline cannot widen an ingress capability. Host simulation/capture
time mapping, including ON_ARRIVAL, happens outside authority locks and never
authorizes TTL. A test-only reentrant simulation callback must exercise the
production callback-context guard and receive BUSY for recursive ingress rather
than deadlock; no such guard substitutes for the trusted native-pointer contract.

### Production ownership and private fixtures

Runtime owns one source-authority context and its commit/admission/security-clock
gates. Source/admin/character authority records use the existing typed registry
and direct leases; token lookup merely identifies a candidate record, never
authorizes dereferencing or bypassing current generation/state checks.

An internal CPU-binding constructor consumes an already validated, bounded
semantic catalog and neutral values. It is a future integration seam for a real
asset validator, not a dummy model loader. Tests supply trusted CPU catalogs to
this same production path. Private ComponentFixture definitions may force clock,
entropy, counter, allocation and interleaving faults only in test translation
units. No production reset/seed/backend setter, test export or public character
constructor is added. Public source APIs and runtime_update remain UNSUPPORTED.

Queue/catalog/ledger storage and source control blocks stay alive until all
direct leases and staged/snapshot references drain. Final host frees happen
outside all locks. Retired registry slots retain terminal HANDLE_EXHAUSTED
semantics; source/control/ticket/ordinal counters also never wrap or assert.
Every independently retained source, character or snapshot lease also retains
the runtime lifetime lease; an object pin cannot outlive an unpinned runtime.
Public close counters sum the documented operation/snapshot categories, and
every independently live lease is accounted for without hiding nested owners.

### Clock and entropy backends

Windows uses QPC with cached positive frequency; Linux uses
clock_gettime(CLOCK_MONOTONIC). Check signed inputs, unit conversion and uint64
nanosecond overflow. QPC conversion uses exact checked integer multiply/divide
with portable wide arithmetic; no floating rounding that can reverse samples.
Equal clock samples are valid; every authorization sample is serialized through
the single runtime clock gate. A strict decrease poisons the whole runtime.
No claim of bounded OS scheduling delay or strict physical-time pixel erasure.

Windows entropy uses BCryptGenRandom(NULL,...,BCRYPT_USE_SYSTEM_PREFERRED_RNG)
and links bcrypt; Linux uses getrandom without GRND_RANDOM. Entropy calls and
ledger reservation happen outside commit/pose/source/clock/router locks.
No std::random_device/PRNG/time-based token fallback or token logging.
Native smoke demonstrates backend execution, not a statistical proof of entropy.

The issuance ledger uses bounded append-only entries with immutable token/binding
and atomic eligibility. Mint without locks; serialize only collision checking
and reservation under an issuance mutex outside other gates. Use release/acquire
publication of ready entries.
Final phase-C eligibility commit must not acquire this mutex under source/pose
locks. Exact nonzero equality uses fixed-width operations; bounded lookup still
requires registry pins and a source-locked current-token check. Failed phase C
permanently burns its reservation. No claim of constant-time remote behavior.

Primary backend references checked during preparation:
[QPC](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps),
[BCryptGenRandom](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom),
[getrandom](https://man7.org/linux/man-pages/man2/getrandom.2.html),
[clock_gettime](https://man7.org/linux/man-pages/man3/clock_gettime.3.html).
These support backend selection; they are not implementation evidence.

### Locks, close and rotation

Registration also reserves bounded source/slot capacity before unlocked minting,
then activates under commit->pose->source->clock only after rechecking runtime,
character generation, grant, reservation ownership and original lease expiry.
Final registration activation cannot reuse a pre-mint clock sample. Failure
publishes zero admin/cap outputs, releases storage/capacity safely, and burns any
allocated identity/token reservation. No live partial registration remains.

1. Lookup: router -> handle registry, acquire direct operation leases, release
   both locks before any commit/pose/source gate. No callback under any gate.
2. Ingress preparation/copy occurs before admission. Under the brief admission
   mutex, check OPEN and increment bounded readers, then release the mutex.
   The RAII admission lease spans source mutex -> security-clock gate -> bounded
   queue append plus sequence/quota transaction; no allocator/callback there.
   Its destructor decrements and notifies on every success/error/exception path.
3. Begin-close: require B1 control thread, hold runtime commit gate, set sticky
   CLOSE_REQUESTED under the admission mutex and deny new admissions immediately.
   Close-requested work-pin admission must also fail CLOSED. CV wait releases
   the admission mutex while existing readers drain; it never holds router,
   source, pose or clock locks. Only then transition to QUIESCING under the
   short router lock and revoke records under one pose gate at a time.
   Preserve separate close intent and quiescing state; finish stays BUSY until
   the close operation, all ingress and management pins drain. Ordinary pre-close
   CPU work can still unpin directly, without being an ingress reader.
4. Query is still a management exception and includes its own lease, ingress
   count and (in B3) CPU snapshot count. FENCE_COMPLETE requires close intent,
   completed ingress drainage and drained work; it is not a promise that another
   live management lease permits finish. GPU counters remain truly zero.
5. Rotate A/C: commit -> character pose -> source -> runtime clock gate. A first
   invalidates old authority and captures source/control/ticket/admin/character
   identities. B mints/reserves outside every gate. C samples time again and
   validates every captured identity plus ledger ownership before activation.
   Failure moves only the still-owned reservation to ROTATE_FAILED, never
   overwriting concurrent EXPLICIT_REVOKED/EXPIRED/EXHAUSTED/CLOSED.
6. Explicit revoke: pose -> source, never commit in reverse order. Clock/expiry
   detection: source -> clock, never pose; publish release tombstones instead.
   EXPLICIT_REVOKED and EXPIRED require a new registration, not rotate recovery.

No wall-clock completion deadline is promised for a stalled trusted reader. A
closing attempt never reopens admission to avoid BUSY or make a test succeed.
Keep the R5 zero-new-admissions and no-post-QUIESCING-append linearization, not
an assertion about the wall-clock moment a previously accepted call returns.

## B3 design prepared for its later kickoff

B3 reuses B2's identities, grants, queues, time and authority; it does not mint a
second source state machine. It builds a bounded CPU semantic candidate and
immutable snapshot, not a renderer or a complete animation/retarget engine.

Provenance key is full runtime UID + character handle generation + source handle
generation + uint64 source epoch. Every value AND future retained-state effect
carries the transitive union, capped at 16 entries; never truncate a union to
make it fit. Capacity failure rejects the candidate without installing state.
Diagnostic label, token bytes and raw public handles supplied by a frame are
not lineage authority. Include zero-weight/cancelled contributors if retained
state can affect later frames. Authored neutral/idle constants have empty lineage.

Proposed B3 limits: 256 retained semantic cells/character, 32 deferred triggers/
character, 512 candidate dependency nodes, 64 CPU snapshots/runtime including
retained historical readers, and 4 MiB additional tracked storage/runtime.
The combined B2/B3 tracked byte ceiling is 12 MiB/runtime. Checked limits may
reject an otherwise valid candidate; they never evict a pinned snapshot or
silently drop trigger effects. Source/pose generation overflow is terminal.

Build an immutable input view and stage all filter/trigger/composite writes
outside commit gates. Dependency closure is derived by production typed value/
state combinators over the input graph, not accepted as a caller's optional
provenance list. The minimal CPU scalar blend/delay harness exercises real
provenance and commit components; it does not implement the complete rig,
quaternion retarget, priority mixer, mode switch or visual pipeline.

Final publish: runtime commit -> one character pose -> all contributor source
locks in stable slot order -> security clock gate. Acquire-read tombstones;
check RUNNING, character/source generations, every transitive dependency,
terminal state, bounds and leases against one final monotonic sample. On
success, install the immutable snapshot and the staged persistent-state revision
as one reader-visible transaction. Readers acquire a snapshot/control-block
lease before accessing it; a pose counter alone is not publication safety.

No CSPRNG, allocation, callback, expensive mixing, blocking I/O or destruction
that frees storage under the final gates. Reserve pool slots and lifetime pins
before locking; exchange the old immutable pointer while locked, then release
old owners/free storage after unlocking. Failed validation discards candidate
AND staged writes without incrementing pose generation.

Ingress expiry publishes a release tombstone under source->clock only. S next
acquire-observes it, retires affected caches under S ownership, and recomputes
from clean neutral plus still-authorized inputs (A+B becomes B only). A prior
snapshot can be historical display state, never an unchecked seed for a newer
pose. CPU snapshot pins block runtime finish; zero CPU references are not GPU
completion evidence. Public update/render remain UNSUPPORTED.

The final authorization promise is sample-time. Pausing BEFORE the final sample
past expiry must reject; preemption AFTER a valid sample does not establish
strict wall-clock erasure. No production sleep/hook is added inside the final
critical section; test-only cut points are compiled only into native tests.

## Custody, acceptance and approval

Exact path sets and parent rules are in `M1_B23_IMPLEMENTATION_MANIFEST.json`;
the proposed sets are **33 paths for B2** and **31 paths for B3**.
The acceptance matrix and deterministic traces are in `M1_B23_ACCEPTANCE_MATRIX.md`.
Every original case/oracle/platform is retained in `M1_B23_SECURITY_CASE_PLAN.json`.
All B2/B3 execution statuses are NOT_EXECUTED and all whole-case verdicts HOLD.
CPU component PASS cannot clear original model/adapter/mode/physical GPU cases.

B1 custody currently binds HEAD to its 24-path scope. In B2, preserve its six
tests against landed B1 `535663960c33c0ebe49481a8681b8625c336fafe`, including
mock targets, and add an independent B2 branch/base/path gate. B3 similarly
pins B2 to its actual landed merge and adds its own gate; do not enlarge old
allowlists or remove historical negatives. Preserve B1's ledger scope at its
snapshot while permitting reviewed component evidence in the current ledger.

This documentation branch is not an implementation candidate. Existing
current-HEAD B1 custody is expected to reject its additional prep paths after
commit. No oracle/workflow is loosened here to obtain a misleading green badge.
Existing workflow path filters do not schedule native jobs for these four root
planning documents. Preparation uses explicit manifest/inventory/pin checks;
the landed baseline's CI is historical evidence only.

Same-account preparation and future SELF_AUDIT must disclose
INDEPENDENCE_NOT_AVAILABLE. Owner implementation approval, exact-head merge
approval, public source activation and G07/G08 security clearance are separate.
Rollback of preparation is closing/reverting this docs proposal; rollback of
future source is a normal reviewed revert, preserving B1 and frozen history.
