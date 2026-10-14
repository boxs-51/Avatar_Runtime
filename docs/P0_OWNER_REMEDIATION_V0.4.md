# Owner P0 Remediation R4 — Irrevocable Expiry and Admission/Close Fence

**ID:** ARS-P0-PR2-R4-OWNER-2026-10-14 | **Status:** PROPOSED / independent review pending
**Input:** ARS-P0-PR2-R3-REAUDIT-2026-10-14-02, audited HEAD bd16136dc8994dd0746bf2b652b4703b79b9ba7f.

**Normative order:** R4 supersedes R3/R2/v0.1 on security clock observation, expiry/fault terminal states and ingress-admission vs begin_close. R3 continues to govern rotation_ticket, terminal explicit revoke, source/character epoch reservations and commit/publish fences; R2 continues to govern transitive retained-pose provenance and registry pins, except where superseded. This is an OWNER design proposal and does not establish G00 or implementation security PASS.

## 1. Invariants

- **CLK-01 (global monotonic authorization):** every authorization-related security clock observation occurs while the SAME per-runtime security_clock_gate is held. Read SDK-owned monotonic OS clock and update non-decreasing security_highwater_ns inside the gate, never outside it. If sampled time is less than highwater or OS clock fails, permanently set runtime SECURITY_CLOCK_FAULT for that runtime incarnation. The fault is sticky, cannot be cleared by rollback, retry, token rotation, or admin call. New admissions, activations and pose publications fail closed. Teardown remains permitted.
- **CLK-02 (terminal expiration):** for a source at ACTIVE, the first observed security sample with now_ns >= expiry_ns transitions its source record to EXPIRED under its source_ingress_mutex. The transition revokes the active cap, invalidates pending rotation reservation, increments source/control epochs with checked non-wrap semantics, and tags all related retained state/provenance as invalid. EXPIRED is TERMINAL for that source registration. New trusted registration is required for renewed authority; ordinary rotate/refresh cannot resurrect an EXPIRED source. Queued frames with the expired epoch must be discarded.
- **CLK-03 (publication cut):** publish holds runtime_commit_gate -> character_pose_publish_gate -> source_ingress_mutex in stable source order -> security_clock_gate for its FINAL sample, authorization and immutable PoseSnapshot pointer swap. No user callback, blocking I/O, GPU calls, unbounded work or allocator on the path while gates are held. The final authorization sample and pointer store are one contiguous bounded critical section. A hardware scheduler may preempt between sample and store: the contract promises **validity at sample-time**, not physical deadline/pixel erasure.
- **CLK-04 (no clock fault resurrection):** a security regression/failure observed in any admission or publish poisons the whole runtime. A later smaller timestamp never becomes a valid authorization sample, even if another source would otherwise accept it. Clock poison and expiry invalidation are separate persistent state transitions.
- **CLS-01 (admission-close linearization):** introduce an independent runtime admission_gate (reader/writer lock). An ingress admission owns the shared admission_gate from its final RUNNING check through the atomic source sequence/quota/queue append. begin_close takes runtime_commit_gate then the EXCLUSIVE admission_gate, briefly takes router_lock to set QUIESCING/stop work pins, releases router_lock, invalidates sources/characters under individual pose gates, and then releases admission_gate and commit_gate. After begin_close has linearized, no admission may append successfully. A submit already pinned but not yet in shared admission_gate is rejected. A submit that entered shared admission_gate earlier may finish before begin_close can linearize.
- **CLS-02 (preserved pose fence):** all new pose commits, capability registrations and rotate activation still require runtime_commit_gate. Pre-close pinned update may safely unpin, but cannot publish after QUIESCING. Source explicit revoke continues to use pose_gate -> source_ingress_mutex, without acquiring commit_gate.

## 2. Lock order and reentrancy

Paths:
- Ingress: admission_gate(SHARED) -> source_ingress_mutex -> security_clock_gate.
- Pose publication: runtime_commit_gate -> character_pose_publish_gate -> source_ingress_mutex (for all involved sources, stable slot order) -> security_clock_gate -> immutable pointer swap.
- begin_close: runtime_commit_gate -> admission_gate(EXCLUSIVE) -> router_lock (short state transition, release) -> each character_pose_publish_gate -> its source_ingress_mutex. Do NOT hold multiple character gates at once.
- Rotate A/C: runtime_commit_gate -> character_pose_publish_gate -> source_ingress_mutex; C checks current runtime RUNNING and epoch ticket exactly.
- Explicit revoke: character_pose_publish_gate -> source_ingress_mutex.
- Registry lookup may use router_lock, but releases it before touching any runtime_commit_gate, admission_gate or pose gate; no inverse nested locks.

The security_clock_gate is last and never takes another lock. The SDK OS clock is non-reentrant and nonblocking. The **admission_gate is a short admission commit barrier**, not held during frame parsing, input copying, CSPRNG, any host callback, network or GPU operations; bounded preallocated queue append only. The publish gate likewise cannot invoke arbitrary host code. Lock fairness and close starvation prevention require writer-preference or a bounded explicit closing-intent bit checked before ingress obtains shared admission_gate, so continuous inputs cannot indefinitely starve begin_close. If a bounded queue cannot append without allocation, reserve capacity outside lock and revalidate inside, or return QUEUE_FULL.

## 3. Clock observation and irrevocable revocation state machine

At the security_clock_gate:
1. If SECURITY_CLOCK_FAULT already set, reject.
2. Call the SDK-owned OS monotonic clock while gate held. If failure or now_ns < security_highwater_ns, atomically set SECURITY_CLOCK_FAULT (sticky); reject.
3. Else set security_highwater_ns = now_ns; decide lease now_ns < expiry_ns (exclusive upper bound).
4. If expired, holding source_ingress_mutex mark EXPIRED and revoke epoch/cap/rotation and source-local + transitive cache lineage. Character-level invalidation epoch propagation must remain bounded and observed by pre-publish provenance checks. No future action under that source admin handle may activate it.
5. Only if clock, runtime, generation, scope, lease and budget are valid may the ingress append/charge or S publish occur.

**Global poison lifetime:** source cache/pose authorization consults the poisoned runtime status. Since no future publish is authorized in this runtime, bulk cache cleanup can happen on next safe simulation/close; its visibility is not a route to a new pose commit. New runtime creation must allocate a new nonreused runtime_uid. There is NO clock reset API for a poisoned running runtime.

**Temporal promises:** a pose published after wall-clock expiry may have been valid at the final security-time sample if a scheduler paused between sample and pointer store. This is expressly permitted only by sample-time semantics; a future clock sample at/after expiry transitions the source to EXPIRED and bars later new pose commits. STRICT display erasure needs a separate render/present fence outside P0 scope.

## 4. Contract-model negative cuts and expected evidence

- **RACE-Q (SRC-038):** epoch active, expiry=200; publish at 150 commits P; sample 200 denies and *terminally expires*; clock regresses to 120; attempt publish of old epoch never commits, pose_generation unchanged, capability invalid, state EXPIRED or runtime CLOCK_FAULT. Repeat with regression before expiry: sticky poison rejects future timestamps, even if time advances.
- **RACE-R (SRC-039):** ingress holds shared admission_gate and pauses after RUNNING check but before append. Concurrent begin_close attempts exclusive gate and MUST wait. Ingress commits BEFORE close linearizes, then close changes state to QUIESCING. After close, stale or fresh ingress cannot enqueue/return OK. Test inverse: close linearizes first, paused prevalidated submit denied. Use deterministic latches, not sleeps.
- **RACE-S:** security high-water shared across source admission and pose publication. Reverse scheduling of calls must not backdate authorization; all clock observations must be serialized with the clock gate.
- **RACE-T:** repeat close while concurrent ingress across many sources and repeated submits; demonstrate no starvation, no accepted admission after state change, no source epoch resurrection. Model is an illustration, not a production soak proof.

Previously required HDL/SRC/RACE suites remain HOLD for production runtime until a real handle registry, motion engine, token ledger, sanitizer evidence and backend-specific GPU cleanup exist. ABI stub and contract-model CI PASS do not meet these requirements.

## 5. Required ADR / ABI / acceptance updates

- **ADR-0005-R4:** runtime security_clock_gate + global watermark, sticky CLOCK_FAULT, terminal EXPIRED, checked non-wrap epochs, sample-time publication.
- **ADR-0004-R4:** runtime admission_gate shared/exclusive, begin_close writer-preference/intent, prior R3 commit/pose barriers and router lock ordering.
- **ADR-0011-R3** retains terminal explicit revoke and phase-C reservation compare; new EXPIRED state cannot be reactivated by rotate.
- Public draft ABI has typed status codes for EXPIRED and SECURITY_CLOCK_FAULT; no new production API is authorized. Future implementation must define error normalization and admin control consequences.
- Update the canonical spec, ADR register, gate matrix and previous R3 pointer to this addendum. Independent review required on the new exact HEAD. **DO NOT MERGE**.
