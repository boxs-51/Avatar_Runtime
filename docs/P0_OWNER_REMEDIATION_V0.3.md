# P0 Owner Remediation R3 — Rotation Reservation and Runtime Publication Fence

**Status:** OWNER PROPOSAL / NOT INDEPENDENTLY APPROVED. **Input audit:** ARS-P0-PR2-R2-REAUDIT-2026-10-14-01 at a9ab2a60a3f83196eae08cc5de995865c5f579cd.

> **R4 SUPERSESSION:** docs/P0_OWNER_REMEDIATION_V0.4.md is authoritative for security clock highwater/fault, irreversible source EXPIRED state and admission vs begin_close fence. Unmodified R3 epoch reservations/terminal explicit revoke remain active. This older document must not be implemented in conflict with R4.

**Normative precedence:** R3 overrides conflicting R2/v0.1 text for rotation vs revoke, TTL and runtime-close publication; unchanged R2 provisions remain in force. NO MERGE and NO G00 PASS.

## R3.1 Lock order and linearization

Relevant locks: runtime_commit_gate -> character_pose_publish_gate -> source_ingress_mutex. The process router lock is acquired briefly under runtime_commit_gate only by begin_close, then released BEFORE pose gates; ordinary handle lookup releases router lock before ever acquiring runtime_commit_gate. Ingress uses only source_ingress_mutex. Revoke uses character_pose_publish_gate -> source_ingress_mutex and MUST NOT acquire runtime_commit_gate while holding either. No callbacks, GPU operations, CSPRNG or blocking I/O inside locks. Do not acquire multiple character pose gates simultaneously.

- ADMIT linearizes at bounded queue append + accepted_seq/quota advancement while source mutex held.
- ROTATE phase A linearizes at invalidation of old cap, increment epoch and reservation ticket under runtime_commit_gate -> pose gate -> source mutex.
- EXPLICIT REVOKE linearizes at source epoch/control epoch advancement, invalidation of active token and pending rotation ticket, and state EXPLICIT_REVOKED under pose gate -> source mutex.
- ROTATE phase C linearizes at activating new token only after comparing exact phase-A ticket, reserved epoch, control epoch, admin generation, character generation and RUNNING runtime, while holding runtime_commit_gate -> pose gate -> source mutex.
- POSE PUBLISH holds runtime_commit_gate -> pose gate -> (brief source locks if needed), validates transitive provenance and authorization time, atomically installs immutable pose. No post-QUIESCING publish permitted.
- BEGIN_CLOSE linearizes under runtime_commit_gate and router lock at RUNNING -> QUIESCING plus disabling new work-pin admission. It then invalidates each character/source under its pose gate before returning.

Any work pinned before begin_close may finish CPU bookkeeping, but cannot commit a pose after begin_close linearizes. Closing does not wait under any publish gate for rendering/GPU completion.

## R3.2 Source state and fail-closed rotation (P0 R2-SEC-01)

State machine: ACTIVE -> ROTATING_OLD_REVOKED -> ACTIVE_NEW or ROTATE_FAILED; from ACTIVE/ROTATING_OLD_REVOKED/ROTATE_FAILED an EXPLICIT REVOKE -> EXPLICIT_REVOKED; from any state admin release or runtime close -> CLOSED. EXPLICIT_REVOKED and CLOSED are terminal for this source registration/admin handle. A new explicit host authorization requires a NEW source registration; retry after a failed rotate is permitted only in ROTATE_FAILED, NEVER after EXPLICIT_REVOKED.

Per-source metadata: monotonically increasing source_epoch, control_epoch, rotation_ticket, reserved_epoch, admin_generation; all overflow fails closed and no ID reuse. Token lifetime nonreuse ledger is bounded.

Rotate phase A: while holding runtime_commit_gate, pose gate and source mutex, validate RUNNING, same admin/character and ACTIVE or ROTATE_FAILED, invalidate old token, advance source and control epoch, reserve fresh rotation_ticket and reserved_epoch, invalidate transitive cached lineage, set ROTATING_OLD_REVOKED. Release ALL locks.

Phase B: mint fresh 128-bit OS-CSPRNG token and ledger reservation OUTSIDE locks. Failure must zero output; under gates, if SAME phase-A reservation still current move to ROTATE_FAILED. If explicit revoke/close already superseded it, preserve EXPLICIT_REVOKED/CLOSED and return REVOKED/CLOSED. Never restore old cap.

Phase C: under runtime_commit_gate -> pose gate -> ingress mutex check state==ROTATING_OLD_REVOKED, rotation_ticket==captured_ticket, source_epoch==captured_epoch, control_epoch==captured_control_epoch, admin_generation unchanged, character generation unchanged, runtime RUNNING, issuance ledger reservation still owned. On match activate token and clear reservation. If any check fails, retire new token eligibility, zero output, leave terminal revoke/close unchanged. A valid admin handle alone never authorizes phase C.

Explicit revoke: invalidate pending rotation ticket and advance control/source epoch under pose gate + mutex, set EXPLICIT_REVOKED, revoke cache/filter/trigger lineage; repeated revoke returns ALREADY_REVOKED/OK without reactivation. Admin release performs terminal revocation before releasing admin handle; if unable to complete, returns BUSY and retains admin authority.

Linearization guarantee: If phase C occurs first, later explicit revoke revokes fresh cap. If revoke occurs first, phase C MUST fail. Never activate a token from a pre-revoke pending rotation after revoke has returned.

## R3.3 Security clock and TTL (P1 R2-SEC-02)

SDK-owned nonblocking monotonic OS security clock is distinct from host animation/simulation clock. Ingress samples security_now INSIDE source ingress_mutex and checks now>=last_security_seen and now<expiry_ns; a regression or failure fails closed and cannot refill quotas. Advance last_security_seen only with monotonic valid observations. Expiry is computed with checked-add and REJECT on overflow, not saturate-to-max.

Pose publication samples security clock inside runtime_commit_gate + pose gate, after expensive candidate computation, immediately before authorization validation and atomic pointer store. All transitive source epoch/lease checks use this single final sample. No user callback, allocations or blocking work between final sample and swap.

**Honest time guarantee:** A newly committed pose is authorized only if all contributing sources were valid at the final security-time sample and no revoke/close linearized earlier. This is NOT an unconditional physical-time promise: scheduler preemption could occur after the sample but before the pointer store and make pixels visible after wall-clock expiry. Next authorization sampling must reject expired provenance; users who need strict display scrub need a separate render/present fence. Test both a pause BEFORE final time sample (expired must reject) and a synthetic pause AFTER final sample (documents allowed time-snapshot semantics rather than asserting physical-time cutoff).

Ingress clock reversed observation order must not allow stale samples to extend source TTL/quota. Mock clock regressions return CLOCK_REGRESSION/CLOCK_FAILURE; no fallback to frame timestamp or host callback.

## R3.4 Begin-close publication fence (P1 R2-SEC-03)

All new pose commits and capability activations/source registrations enter runtime_commit_gate. begin_close enters same gate, takes router lock briefly, changes runtime RUNNING -> QUIESCING and disables work-pin admission, then invalidates all source generations/rotation reservations through character publish gates (one character at a time), and releases runtime_commit_gate before returning. A pre-close update pin waiting OUTSIDE commit_gate resumes and fails CLOSED; an update holding commit_gate may commit before begin_close linearizes, never afterward. Publish checks runtime RUNNING while holding commit_gate. Finalizer/close-management pins and GPU resource retirement remain governed by R2.6 and undecided D3D11 ADR-0001.

## R3.5 Tests, ABI and remaining gates

- RACE-L: rotate paused between A and C, explicit revoke completes, phase C must fail; also admin_release and begin_close variants.
- RACE-M: pre-close update operation pinned but paused before publish; begin_close completes -> no pose commit.
- RACE-N: concurrent ingress security samples/lock acquisition and clock regression; no backdated quota/lease.
- RACE-O: source TTL expires before final publish time sample; candidate rejected.
- RACE-P: pause after valid final sample, advance clock to expiry; verify the documented sample-time (not strict wall-clock) guarantee.
- Keep retained provenance RACE-F, atomic duplicate sequence RACE-G and revoke-vs-ingress RACE-H.
- Fixed-width C ABI continues to be draft. Extend stub export/link coverage for every public symbol, plus layout assertions. Stub runs do NOT implement security.
- Contract-model tests may check state-machine interleavings, but production runtime HDL/SRC/RACE tests, sanitizer, Windows GPU and ADR-0001 remain HOLD.
- Update ADR-0004/0005/0011 and canonical spec/gates references together. Obtain fresh independent DESIGN review at exact updated HEAD. DO NOT MERGE.
