# Owner P0 Remediation R2 — Normative Contract Proposal

**Stage:** M0 design-only; proposed, not G00-approved.
**Audit input:** ARS-P0-PR2-AUDIT-2026-10-13-01, candidate 1891b711df8f474ff34241382b0e0268c07bb81d.
> **R5 SUPERSESSION:** docs/P0_OWNER_REMEDIATION_V0.5.md supersedes earlier close-liveness, expiry dirty-provenance and counter-exhaustion wording.

> **R4 SUPERSESSION:** docs/P0_OWNER_REMEDIATION_V0.4.md governs expiry and admission-close and has highest precedence over R3/R2/v0.1.

> **R3 supersession (2026-10-14):** docs/P0_OWNER_REMEDIATION_V0.3.md is the authoritative amendment for explicit revoke vs pending rotate, rotation reservation epochs, dual-clock TTL sample semantics, runtime-wide begin_close/publish commit_gate, and lock hierarchy. R2 continues to govern transitive provenance, atomic ingress sequence, close-management and ABI rules only where compatible with R3. No design or implementation approval is claimed.

**Precedence:** This document amends and takes precedence over conflicting details in P0_HANDLE_AND_SOURCE_AUTHORITY_CONTRACT_V0.1.md until the contracts are consolidated. All changed interfaces are still DRAFT.
**No production implementation, no merge approval, no security gate PASS is implied.**

## R2.1 Concurrency topology and linearization points

The host is trusted. Native SDK clients that can execute arbitrary code in the same address space are not security-isolated by the SDK. Network packets are decoded/authenticated by a separate trusted adapter.

Four operations have explicit security linearization points:

1. **ADMIT:** A per-source ingress mutex serializes check of active token/epoch, character generation, sequence high water, resource budgets, lease and quota, followed by bounded queue append; success updates high water and quota together. Mutex release makes accepted queued frame visible. No other admission for the same source can interleave this critical transaction.
2. **REVOKE/ROTATE:** Per-character publish gate is acquired before any ingress mutex (global lock order). Under this gate, the source ingress mutex is acquired and the active capability generation is invalidated. Return occurs only after invalidation and publish gate release. Any frame admitted earlier can be discarded later, but must not contribute to a new committed snapshot after revocation.
3. **PUBLISH:** Simulation acquires the SAME per-character publish gate, samples the SDK-owned security monotonic clock, validates every transitive provenance dependency and live lease before installing an immutable pose pointer with release semantics; the pointer swap is the commit linearization point.
4. **CLOSE:** Runtime router lock changes RUNNING to QUIESCING and disables new work-pins atomically. Existing pinned work may finish memory-safe; new pose commits from invalidated characters/sources are disallowed.

No ingress operation may acquire publish gate while holding a source mutex. Source acquisition order for multi-source revocation uses stable source slot ordering. Publish gate may acquire ingress mutexes in stable slot order for brief read-only authorization checks. Heavy parsing, network callbacks, GPU calls, full mixing and memory deallocation are forbidden under publish gate and ingress mutex. A callback never runs while ARS holds any internal lock.

## R2.2 Provenance-aware motion state and revoke (fix P0-PR2-01)

A motion state is **source-dependent** whenever it is causally derived from an input source frame, including after interpolation, smoothing, retargeting, blending, animation triggers or pose history. The provenance is a bounded set of (runtime_uid, character_slot, character_generation, source_slot, source_epoch), not the external diagnostic label. No source-dependent cache may silently discard this provenance.

**Required separation:**
- Each source owns source-local input queue, de-jitter buffers, filter/smoothing history, calibration adjustments and continuous trigger state, tagged with its own epoch.
- Character state owns source-independent authored neutral/idle/keyframed behavior, plus source-composite pose/caches. Composite caches track the UNION of all transitively contributing epochs, even if numerical values cancel or weight is currently zero but its retained state can affect a future frame.
- Mode-switch retarget caches, morph/mesh parameters, physical integrators and delayed animation effects must either carry the same transitive provenance or be reset to source-independent baseline on source invalidation. Untracked derived state is forbidden.
- An immutable PoseSnapshot carries a provenance set and character generation in addition to pose_generation. Last-published pose is historical display state ONLY. It is not a default input to next-pose computation unless its provenance has been revalidated at the new commit.

**Revoke algorithm:**
A. Take per-character publish gate. Under it take that source ingress mutex; set REVOKED, increment source epoch, detach source from future authorization; release ingress mutex.
B. Under publish gate set character invalidation epoch / dirty mask. Mark all source-local data, queued triggers and transitive cached state depending on the revoked epoch INVALID (O(1) epoch-tagged mark; actual clearing later by S). Release gate and return.
C. Next S update first reconciles dirty epochs, discards stale filter/trigger queues and recomputes affected channels from neutral/idle and remaining authorized sources only. It MUST NOT seed a new channel with the last published pose if it depended on a revoked epoch.
D. Candidate construction is outside publish gate and MUST stage, not mutate in-place, any filter/pose persistent-state write. Candidate includes provenance closure of its new states and value outputs.
E. Before publishing, take gate, read the internally managed security monotonic clock, verify runtime/character generation, lease expiry and every source epoch for ALL candidate value + future-retained state provenance. If invalid, discard candidate AND its staged state writes. Recompute only from clean baseline/valid contributors.
F. On successful commit, atomically install both the snapshot and staged persistent-state update under single-writer S ownership with release ordering; R acquires an immutable snapshot pin with acquire ordering. Never republish old source-tainted data with a newer pose generation.

**What is allowed:** A pose published before revoke, or pixels already submitted before revoke, may remain visible until a subsequent render/present fence. No post-revoke pose generation or newly published state may include revoked provenance, even through cache/mode switch/spring integration/delayed actions. Visual scrub is a separate explicit operation, not guaranteed by revoke.

**Boundedness:** max active sources 16; provenance cardinality capped by active authorizable source epochs and finite dependency graph. Invalid old generations must be dropped from all mutable caches during next update. Resource allocations for provenance use preallocated or bounded storage, never unbounded linked ancestry.

## R2.3 Atomic per-capability sequence admission (fix P0-PR2-02)

The source ingress mutex guards ALL fields for that capability generation:
- active epoch/token and bound character generation
- accepted_high_seq, queue generation, bounded queue occupancy
- ingress rate token bucket and byte/frames budget
- queued frame enqueue position and data-ownership handoff

**Preparation outside mutex:** validate C buffer pointer preconditions, struct versions/counts, copy full frame to bounded temporary and validate finite values/canonical allowed-channel shape (authorization is repeated inside mutex). Read trusted security clock for cost/rate only through an SDK-owned monotonic primitive, not a host callback.

**Atomic admission transaction:** under source mutex verify ACTIVE token and character binding, current epoch, scope, TTL, frame typed values, sequence strictly GREATER than accepted_high_seq (initial 0; first sequence at least 1), and quotas/capacity. Append the immutable owned frame with stamped epoch and monotonic accepted-order ordinal. ONLY after append success, atomically update accepted_high_seq, charge tokens and return OK. A failed validation, allocation, queue append or quota check must not advance high water or charge quota. Reject older arriving sequences—even if their timestamps are within reorder window—rather than reordering accepted ingress. Consumers process queue in admission/sequence order; timestamp jitter interpolation is bounded within accepted order and cannot rewind pose.

**Race with revoke/rotate:** source revoke/rotate acquires publish gate BEFORE ingress mutex. A submit that acquires ingress mutex first may complete queue append; a subsequent revoke still invalidates its epoch before returning, and the S consumer/publish gate rejects it. A submit that locks source after revocation fails. No wait for submit while holding a callback or GPU lock. Timestamp mode ON_ARRIVAL overwrites untrusted timestamp with runtime capture time. UINT64_MAX is final legal sequence if strictly larger than previous; after it, rotate is required. Drop old cap and sequence upon rotation.

**Counterexample required:** two I threads prevalidate identical sequence 100 while last_seq=99; exactly one appends, one rejects, and neither breaks quota. Higher 101 accepted first makes later 100 reject. If queue is full and append fails, 102 retry remains admissible after capacity frees; rejected 102 must not consume sequence.

## R2.4 Fail-closed capability rotation and issuance (fix P1-PR2-03)

**Security-sensitive rotate is revoke-first**, never an attempt to preserve the previous token on failure. New states: ACTIVE -> ROTATING_OLD_REVOKED -> ACTIVE_NEW or REVOKED. A ROTATING source is denied for ingress; old cap is revoked under publish gate + source ingress mutex BEFORE calling CSPRNG.

- Phase A: revoke old cap, increment epoch and invalidate derived state (same barrier as R2.2); return from that phase only after old token is unusable for any new pose.
- Phase B: outside locks obtain cryptographically random new 128-bit cap from OS, check nonzero and lifetime nonreuse ledger; prepare any allocations, new queue/seq state. No weaker PRNG fallback.
- Phase C: under publish gate + ingress mutex check admin/source still current, not CLOSED/QUIESCING, ledger reservation owned; publish new token and ACTIVE_NEW new epoch. No scope widening. Out cap assigned only after commit.
- Failure in B/C: source remains REVOKED, old token remains INVALID, output cap zero, caller receives an explicit error; retry with trusted admin through rotate-after-revoke or perform a new registration. Never reenable old cap.
- Token lifetime nonreuse ledger is bounded; exhaustion yields HANDLE_EXHAUSTED and remains fail-closed. Do not rely only on collision probability for a normative nonreuse guarantee.
- admin_release on ACTIVE performs the same revocation barrier before final release; if revocation cannot complete, return BUSY and keep admin handle, NEVER silently drop last management authority.

## R2.5 TTL at actual pose publication (fix P1-PR2-04)

Distinguish **security lease clock** from **simulation clock**:
- Security lease/authorization/rate uses SDK-owned non-reentrant, nonblocking, monotonic OS clock, callable while holding short internal locks, not host callbacks. QPC/monotonic-clock behavior and platform failure must be specified in the backend adapter. It returns an integer nanosecond value or CLOCK_FAILURE. A backward sample, arithmetic overflow or unavailable clock MUST fail closed (reject admission / do not publish); an expiry comparison is inclusive: valid only if now_ns < expiry_ns.
- Host simulation time (runtime_update now_ns) controls animation sampling only. Host clock callback remains optional for source time mapping; it MUST NOT authorize cap TTL and is always called without ARS locks. Two clock domains must never be mixed without a documented conversion.
- Compute expiry_ns at registration or approved trusted lease refresh, with checked saturating arithmetic: if overflow, reject registration/refresh instead of wrapping.
- PUBLISH samples the security clock inside publish gate immediately BEFORE commit and verifies all sources at that sample. If processing was paused across expiry, candidate is rejected even when it was constructed with an earlier valid timestamp.
- Lease expiry triggers the same source invalidation and provenance invalidation semantics as REVOKE; if detected during publication while gate held, perform bounded epoch invalidation and defer large cache cleanup to S. Clock failure cannot retain an old source in a newly committed pose.

## R2.6 Exact runtime close and GPU lifetime (fix P1-PR2-05)

Runtime transitions RUNNING -> QUIESCING -> DRAINING -> GPU_RETIRING -> FINALIZING -> DEAD. Each operation has an internal RAII operation lease (not requiring a public handle lookup to UNPIN). Categories: ingress, control/update, source admin, close-management, render snapshot, GPU work/retirement. The close state exposes total_pins = SUM of category pins, plus render_snapshot_pins, ingress_pins, gpu_pending_count; all counters must be auditable, not necessarily exact concurrently observed snapshots outside registry lock.

- Begin_close: atomically close work-pin admission in process router; revoke all bound sources and characters using the publish-gate protocol, and cancel queued work; existing pinned operations are allowed to finish safely.
- Close-management entry is allowed only for query/drain/finish while QUIESCING..GPU_RETIRING. query obtains its pin under router lock, reads snapshot of state and releases its lease even if public runtime handle is logically invalidated.
- drain_gpu is exclusive to R thread; it operates only when GPU device is alive, retires behind confirmed backend-safe GPU completion, and never does GPU work on H or calls arbitrary host callbacks under internal locks.
- finish_close acquires an EXCLUSIVE finalizer claim in router before examining quiescence. If another finalizer holds claim, return BUSY without mutating state. While claim is active, new query/drain close-management pins are temporarily barred; finish returns BUSY when pins or GPU retirement remain (then releases claim to permit future drain/query), or transitions to FINALIZING, atomically unlinks router entry with no active management leases, and reclaims control blocks.
- Caller with a stale runtime handle is rejected by fresh lookup, but previously pinned work MUST still unpin through its direct control block lease even after router unlink; finish cannot unlink until every lease has retired.
- Destroying a Character invalidates its public slot, but GPU resources are preserved until render snapshots drop pins AND a device completion fence/query reports safe retire. Do not conflate a C++ snapshot refcount becoming zero with GPU command completion.
- The precise D3D11 completion mechanism and host device ownership remain subject to ADR-0001; **there is no G00 proof of GPU cleanup until that ADR and evidence exist**.

Close linearization invariants: no new work after begin_close; one finalizer wins; no query pin is stranded by unlink; no resurrection; GPU R affinity enforced. Do not spin or block the control thread while holding router/source/pose locks.

## R2.7 C ABI and pointer trust (fix P1-PR2-06/07)

Interface is in contracts/ars_p0_contract_draft.h. For draft cross-platform ABI:
- status is typedef int32_t, status constants defined separately; never expose enum width across DLL boundary.
- ARS_P0_API export/import visibility; ARS_P0_CALL calling convention macro, explicit C linkage, Windows MSVC x64 and GCC/Clang targets.
- Version = 16-bit major + 16-bit minor in uint32_t; reject major mismatch; major=1/minor=0 baseline, unknown reserved flags reject; validate struct_size BEFORE reading optional tail fields, never read beyond declared struct size. No implicit packing pragmas; compile-time layout/offset tests required on both platforms.
- C API callers are trusted native code. They MUST provide valid readable/writable buffers for declared byte lengths for the entire synchronous call, correctly aligned pointers and valid function pointers. No C library can reliably reject an arbitrary non-NULL invalid process pointer. SDK validates NULL/count/overflow/size, NOT arbitrary page accessibility.
- Untrusted API/camera/network serialized payloads MUST pass a separate bounded decoder in host adapter or restricted IPC worker before obtaining trusted C pointers; malformed serialized lengths/refs are rejected without constructing C structs.
- Capability redaction: never deliberately log tokens or transmit in remote payload; scrub adapter-held tokens on retirement where practical. A memory dump may still contain tokens; protect/disable sensitive dumps via host policy. The SDK cannot promise that process memory or crash dumps never contain a bearer token.
- A linked plain-C ABI fixture using a deliberately unsupported stub tests calling convention and symbol layout ONLY; it does not demonstrate registry/security behavior.

## R2.8 Remaining review/implementation boundaries

- ADR-0001 D3D11 device ownership/retire is UNDECIDED. ADR-0004 threading+locks and ADR-0005 dual clocks are R2 proposed and must be independently accepted in canonical ADR register before G00 PASS.
- Regression gates RACE-F/G/H, SRC-023/024 and further CLOSE/CLOCK/ROTATE cases are mandatory. Dynamic implementation tests, sanitizer reports and CI evidence still HOLD until executed.
- Every owner implementation branch must pin an exact source manifest and report actual compiler/OS/CI evidence. The present PR remains document/contract preparation; it does not authorize production or a merge.
