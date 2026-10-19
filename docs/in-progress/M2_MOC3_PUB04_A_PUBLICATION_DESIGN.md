# PUB-04-A: core-authorized packet publication, C0

User: "tiep tuc muc tieu tiep theo" after PUB-02-B `9a9ccd6`. Deliver CODE_CHANGE:
integrate actual core authorization with packet-only GPU publication. This is a
bounded continuation of PUB-04, not completion of the public product.

## Mandatory observations

* PUBA-01: host prepares an actual core candidate for a trusted normalized request
  before receiving worker output. Bind request sequence, expected nonce, batch
  record index/clock and bounded input lease. Host parses only the independently
  typed output protocol, never MOC/PNG/Physics3 bytes or model catalog metadata.
* PUBA-02: exact GPU query completion precedes final core authorization. A pinned
  nonthrowing fence runs before changing the host active texture/epoch. Only core
  success accepts the physics batch cursor and image. No allocation or fallible
  work occurs between successful core publish and the active-target swap.
* PUBA-03: worker failure, failed reload/invalid or stale response, cancellation,
  lease expiry, late source revoke and GPU allocation failure preserve prior CPU
  generation, target identity/epoch, accepted physics cursor and actual pixels.
  Failed prepared sessions are terminal: never silently rotate/remint authority.
* PUBA-04: generated packet/control negatives and actual isolated local physics
  batches on WARP/hardware; compare published pixels to the already-qualified
  packet renderer. Observe real late revoke at query-ready boundary, real elapsed
  lease expiry and actual worker crash. Visible window/API show accepted frames;
  failed candidates never appear there. Independent review and exact run pins.

## Method and boundaries

Add an optional owned `PublicationFence` to the private D3D11 host's pending MOC
transaction. Keeping the fence on Pending prevents an alternate no-argument poll
or close path from bypassing authorization. Reject/close only releases candidate
pins after exact completion; unknown GPU completion remains process quarantine.
Earlier 2D/3D/MOC callers without a fence retain their behavior and are regressed.

A packet publication owner uses the existing CoreConsumer with one generic
normalized semantic channel. This authorizes the trusted caller's request, not a
model-controlled parameter ID or a fidelity assertion about hostile worker data.
Only a live prepared request can select its exact record; caller supplies its
identity before output. Worker output still needs full independent validation.
The opaque batch remains speculative until individual acceptance. Its physics
cursor is host acceptance bookkeeping, not a serialized particle-state authority.

Alternative: publish the core after `poll_commit()` swaps the target. Rejected
because late revoke could expose a new image before final authorization. Running
the model/physics in the trusted host is also excluded by the existing boundary.

Input lease uses owner-side monotonic time (100..2000 ms), checked before GPU
submission and again in the final fence. Relative worker timestamps are not
leases. Source revocation uses actual core authority. A trusted diagnostic pipe
can exercise these operations; there is no new public upload/control endpoint or
SDK ABI. Tokens remain private/native and never appear in diagnostics.

The tested short batch does not provide a long-lived isolated evaluator with
prepare/commit acknowledgement. Full PUB-02 and the final worker particle-state
ack/cancel/reload protocol of PUB-04 remain HOLD, as do SDK/qualification and
physical D06/D08. Existing worker512 MiB/CPU2s/wall5s/output192 MiB limits stand.
No merge, public activation, model redistribution or GPU reset is authorized.

## Plan and document reconciliation

1. Add owned final-fence host hook and packet publication owner; generated tests
   cover old/new host behavior and all failure before/after query boundaries.
2. Add bounded trusted diagnostic pipe and visible/API verifier. Prepare a live
   grant before actual admission/crash; retain previous state/pixels on negatives.
3. Regress core/host and static/parameter/physics modes; independent review, scoped
   evidence/source closure, current-state ledger/catalog updates, commit/push.

Baseline README/ledger are CURRENT_STATE; public pipeline is the planned target;
PUB-02-B contract/evidence describe the completed bounded physics method and are
not retroactively changed. This contract derives from the user continuation and
the next recorded missing boundary. Original approval/activation limits persist.
