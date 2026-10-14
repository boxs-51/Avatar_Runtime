# ARS P0 Design ADR Pack (Owner Proposal)

**Status:** PROPOSED, REQUIRES INDEPENDENT SECURITY REVIEW. No ADR here is accepted by auditor approval, nor does this unlock M1.
**Audit:** ARS-P0-PR2-AUDIT-2026-10-13-01
**Normative implementation design:** docs/P0_OWNER_REMEDIATION_V0.2.md
**Architecture context:** docs/ROADMAP_AND_ADRS_V0.1.md and docs/SPECIFICATION_V0.1.md

## R4 ADR amendments (OWNER PROPOSED — NOT ACCEPTED)

- **ADR-0005-R4 — monotonic security authority:** One runtime security_clock_gate serializes all admission and pose-publish OS monotonic clock samples and advances runtime security_highwater_ns. Any regression/failure permanently poisons that runtime, denying admission/activation/publish. Source expiry is terminal EXPIRED with epoch/capability and transitive cache invalidation; no refresh/rotate reactivates it. New trusted registration required.
- **ADR-0004-R4 — exact admission/close fence:** Ingress uses admission_gate SHARED -> source_ingress_mutex -> security_clock_gate; begin_close uses runtime_commit_gate -> admission_gate EXCLUSIVE -> router_lock (brief) -> character_pose_gate -> source_ingress_mutex. An atomic close-intent bit prevents new readers from starving exclusive close. Admissions holding the shared gate can commit only before close linearizes; no post-close successful queue append. Preparsed frames not yet admitted are rejected.
- **ADR-0011-R4 interaction:** R3 terminal explicit revoke and phase-C rotation reservations remain mandatory, but EXPIRED/SECURITY_CLOCK_FAULT cannot be used as a recovery state by rotate/registration within a poisoned runtime. Both rotation phase A and phase C must read the security clock under the source mutex/security_clock_gate; expiry during unlocked CSPRNG phase B is TERMINAL at phase C and defeats the pending token.
- **Evidence:** RACE-Q/R/S contract-model and SRC-038..041 are design-only. Real runtime and GPU ADR-0001 remain HOLD; require fresh independent review.

## R3 ADR amendments (OWNER PROPOSED, NOT ACCEPTED)

- **ADR-0004-R3:** A runtime-wide commit_gate is always acquired before character pose_publish_gate for pose commits, source activation and begin_close. Character gate precedes source ingress mutex. Brief router_lock can be acquired under commit_gate only by begin_close; ordinary lookup releases router lock before waiting on commit_gate. begin_close sets QUIESCING and closes work-pin admission under commit_gate, and any pre-close update not yet committed is barred from publishing afterward. Tests RACE-M.
- **ADR-0005-R3:** Lease/ingress quota samples use SDK monotonic security clock under per-source ingress mutex (with last-observed high-water). Pose publication validates lease at a final authorization clock sample inside commit_gate+pose gate, immediately before pointer swap. This guarantees security at sample time, not absolute physical-time pixel erasure in presence of scheduler preemption. Tests RACE-N/O/P.
- **ADR-0011-R3:** Pending rotate stores rotation_ticket, source/control epochs and admin generation at phase A; phase C must compare all and require ROTATING_OLD_REVOKED. Explicit revoke invalidates ticket and is terminal for that admin/source registration. Failed rotate is separately ROTATE_FAILED and may retry only with trusted active admin. Tests RACE-L.
- **ADR-0001 remains unresolved:** host D3D11 device, GPU completion and physically safe retirement need independent decision and native evidence.
- Exact normative owner design: docs/P0_OWNER_REMEDIATION_V0.3.md. All R3 ADR items are PROPOSED and require independent re-audit.

## ADR-0004-R2 — Concurrency and handle lifecycle
- Decision proposed: typed by-value runtime UID/slot/generation handles; registry pin acquisition before payload access; logical close distinct from delayed CPU/GPU reclamation. Control, simulation and GPU threads obey owner rules. Only ingress and security revoke are concurrently callable exceptions.
- Lock order: router -> handle registry only during pin acquisition, release before character publish gate -> source ingress mutex. No inverse nested acquisition; never host callback while holding these locks.
- Finalization: exclusive finalizer claim and close-management pins, BUSY until all operation/snapshot/GPU retire pins complete. No public stale lookup during internal unpin.
- Alternatives rejected: public raw pointers, blocking destroy under arbitrary host callback, asynchronous GPU free on wrong thread.
- Gate: HDL-001..019, RACE-D/J; D3D11 retirement details remain ADR-0001 HOLD.
- Compatibility: affects unimplemented draft ABI; no binary production contract exists.

## ADR-0005-R2 — Separate security authorization clock from simulation clock
- Decision proposed: SDK-owned monotonic OS security clock for source TTL and quotas; read synchronously inside publish gate with no host callback. Host monotonic simulation clock stays for timestamp mapping/update; never authorizes leases.
- Clock error/regression/overflow: reject admission or publication; expiry valid only when security_now < expiry.
- Alternatives rejected: caller-supplied capture timestamp to determine authorization; stale clock sample outside gate; host callback within internal lock.
- Gate: SRC-012/020/025 and RACE-I. Both Linux and Windows native monotonic backends need separate implementation evidence.
- Compatibility: mock tests must inject a controlled clock provider; production provider is OS monotonic and non-reentrant.

## ADR-0009 — Revocation and transitive pose provenance
- Decision proposed: keep per-source filter state, and record transitive epoch union on retained values, filters, trigger queues, body/morph caches and composite pose. On revoke: mark epoch invalid under pose gate; next simulation recomputes from clean neutral/other active sources. No historical revoked value enters new pose_generation.
- Publication requires validity of ALL transitive dependencies at commit. Cached state updates are staged until same validity barrier succeeds.
- Alternatives rejected: validate only direct frame contributor IDs, copy last pose without provenance, clear only ingress queue.
- Gate: SRC-023/024/032 and RACE-B/C/F.

## ADR-0010 — Atomic ingress admission and sequence high-water
- Decision proposed: one per-source ingress mutex transaction for active cap epoch, character generation, strict sequence high water, scoped channel/TTL/quota, queue capacity, immutable-frame append, quota charge and accepted ordinal.
- Reject seq less than or equal to accepted high water; no reordering of admitted frames. Appending failure cannot burn sequence/quota. Timestamp interpolation inside a monotonic accepted stream must not reorder pose-generation commitments.
- Alternatives rejected: separate validate/enqueue without shared mutex, update sequence before admission succeeds.
- Gate: SRC-008/009/027/028/029, RACE-G/H.

## ADR-0011 — Fail-closed cap rotation
- Decision proposed: revoke old token FIRST under publish barrier; issue fresh CSPRNG + lifetime ledger token after; only activate if mint/ledger/policy commit succeeded. Any failure leaves source REVOKED and out_cap zero. Trusted admin can retry securely, not recover old token.
- Alternatives rejected: keeping previous potentially compromised cap active on RNG failure, recycling previously revoked tokens, making admin_release silently drop last active administrator.
- Gate: SRC-004/010/022/026/030, RACE-K.

## ADR-0012 — Fixed-width C ABI and untrusted payload boundary
- Decision proposed: int32_t status return, versioned sizes, ARS_P0_API / ARS_P0_CALL, no C enum return widths, no forced packing. Byte-counted, synchronous trusted pointers must remain accessible during calls. Serialized untrusted inputs are decoded by bounded adapter before C ABI invocation.
- Alternatives rejected: unbounded JSON pointers passed directly to core; claim that arbitrary invalid native address can be safely probed; ABI export by compiler default.
- Gate: compile+link C11 and C++20 consumers against stub shared library on Linux/Windows; genuine security behavior remains HOLD until registry and motion implementation exists.
- ABI limitations: 32-bit Windows, Python/C# bindings, and full major/minor forward compatibility require separate release-level review.

## ADR-0001 still blocks GPU finalization claims

The initial host-owned D3D11 device and deferred-reclamation mechanism remain a distinct pending decision. The R2 close protocol specifies WHAT must be safe, but not a proven GPU synchronization primitive. Do not claim GPU-safe destroy until this ADR and corresponding native GPU tests are closed.

## Sign-off status

Owner-authored contract proposal; no independent reviewer accepted these ADR changes. All runtime negative tests and CI security gates are HOLD until implementation. Architecture G00 stays HOLD.
