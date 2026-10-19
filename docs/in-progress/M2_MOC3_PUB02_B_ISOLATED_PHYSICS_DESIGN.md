# PUB-02-B: isolated retained physics, C0

User continuation: "tiep tuc trien khai tiep nhung phan con lai". This continues
the public-input pipeline after PUB-02-A at `89f732d`; it does not authorize merge,
activation, SDK capability claims, redistribution or physical device reset.

## Frozen outcome and scope

* PHY-01: execute the existing independent bounded Physics3 method natively
  inside the AppContainer worker, retaining particle/clock state and one evaluator
  across a bounded batch. Package/MOC/PNG/Physics bytes never enter the packet-only
  trusted renderer. Preserve the previous static and parameter-only modes.
* PHY-02: fixed 16,666,667 ns steps; complete manual overrides each frame, omitted
  input release, explicit enable/disable/reset, at most eight steps of accumulated
  time. Prepare candidate state/parameters and commit only after evaluator update
  and successful snapshot serialization. Numeric instability or malformed commands
  rejects the entire custody result; previous host verified output is unchanged.
* PHY-03: separate versioned physics output; host independently validates every
  record before scene allocation/GPU. Reject unknown version/flags or physics
  provenance on static/parameter modes. Sequence/nonce/clock and budget checks stay.
* PHY-04: direct local physics ON/OFF and release/reset/replay evidence, actual
  geometry and pixels, same-backend trusted Python-method reference on WARP and
  hardware, authenticated API and mapped window. Generated numeric/transactional
  and malformed-input/output tests supplement actual OS cleanup evidence.

All existing limits remain: 1..8 frames, 32 manual bindings, 512 MiB worker/job,
two CPU seconds, five wall seconds, 192 MiB aggregate output. No new dependency.
Private test derivatives are explicit and do not imply original-size support.
The solver remains an independent method, with no Cubism/author equivalence claim.

## Chosen interface and method

Use native C++ double math for the existing `scripts/moc3_physics.py` algorithm,
reusing strict native Physics3 validation before constructing typed chains. The
alternative of a Python child worker adds an interpreter and bootstrap surface;
running Python physics in the trusted host would violate isolation. The native
method preserves existing validation and can be compared to the Python reference.

Opt-in worker `--simulate` accepts the PUB-02-A envelope and commands. Per-frame
reserved becomes physics control bits: enabled=1, reset=2, no other bits. Existing
`--evaluate` still requires zero. The first clock stays zero; later clocks strictly
increase as before. Reset does not rewind transport clocks: it resets particles,
remainder and elapsed integration at the supplied current timestamp. Disable
returns complete manual/default parameters and rest state, with zero remainder.
Physics-linked data is required in simulate mode; missing/invalid physics rejects.

Output uses `ARSEVAL1` outer version2 and `ARSDRAW1` inner version3. Bit128 marks
physics execution; it is required with the existing linked/validated bits3 for
inner v3, forbidden for static v1 and parameter v2. The flag describes this
worker batch method, not product authorization. Every record, including an OFF
control, belongs to that method; diagnostics label each pose/control separately.

The candidate is owner-thread-bound, generation checked, and not publicly mutable.
Clock/backlog/finite/range checks occur before state publication. All commands are
preflighted before any output. Snapshot bytes may be streamed inside the isolated
job, but the broker only returns custody after full clean success; a mid-batch
failure cannot supply a partial candidate to the host. Full host final authority,
input lease, cancellation/reload and GPU/core/physics commit remain PUB-04 HOLD.

## Plan and proof

1. Add native typed solver + generated boundary/transaction tests; preserve the
   strict parser and Python reference oracles. Reversible optional/default-OFF code.
2. Add opt-in worker/broker mode and independently guarded output version. Regress
   static/parameter protocols, caps and negative-before-GPU behavior.
3. Add visible/API physics verifier with explicit small derivative fixture and
   ON/OFF, released temporal response, reset and deterministic fresh-worker replay.
   Freeze source/binary pins around final runs; keep failed runs, no blind retry.
4. Independent review, scoped documentation reconciliation, completion report,
   commit/push on current branch. Do not close full PUB-02/PUB-04 or product gates.

Document reconciliation at C0: pipeline plan is PLANNED_PROPOSED; release ledger
is CURRENT_STATE; PUB-02-A design/acceptance/source are the completed bounded
contract/evidence; Python solver is CURRENT_STATE for trusted-local method only.
Static admission contract remains effective for static mode (intentional separate
version). No historical evidence pins or private dependency viewer edits change.
