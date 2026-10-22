# PUB-04-B: bounded worker checkpoint acknowledgement, C0

User: "tiep tuc stage tiep theo", after `c925bb0`. CODE_CHANGE continuation of
worker state ACK/lifecycle, preserving the previously approved public-input goal.

* CP-01: native worker restores actual particle/velocity/clock/remainder state
  from the last host-accepted typed checkpoint, acknowledges that exact
  predecessor and returns one new candidate checkpoint plus drawable packet.
  Verify resumed output against continuous independent physics and fresh replay.
* CP-02: checkpoint/candidate protocols have bounded counts/lengths, finite
  doubles, explicit sequence/clock and SHA-256 package/entry identity. Worker
  validates restored shape against its actual linked physics before any update.
  Host independently validates all candidate/checkpoint/drawable framing before
  GPU allocation and binds receipt to the expected accepted predecessor.
* CP-03: the existing query/final-core fence accepts immutable checkpoint, core
  and frame together using nonthrowing assignments. Failed worker, invalid ACK,
  wrong package, stale checkpoint, cancellation, expiry and late revoke preserve
  the prior actual checkpoint bytes and frame. Prepared failure remains terminal.
* CP-04: generated numeric/protocol/transaction negatives and actual local
  resumed-worker runs on WARP/hardware, pixel parity with the continuous method,
  visible/API observations, independent review and exact source custody.

## Chosen bounded method

An immutable checkpoint is the authoritative accepted physics state. Each fresh
AppContainer job restores it and returns a candidate plus ACK of the predecessor
digest. The host can accept that state even after clean worker exit; there is no
fallible post-core-publish IPC. A failed/late job cannot overwrite the accepted
checkpoint. This extends PUB-04-A's cursor into actual restorable solver state.

Alternative: duplex live-worker COMMIT after host publication. Rejected for this
stage because worker death/ACK loss introduces a fallible operation after core
publication and a long-lived worker violates the unchanged five-second job cap.
A persistent isolated evaluator remains the eventual PUB-02 target, not a claim
of this checkpoint bridge. Model/package decode is repeated inside each job.

The host stores/validates typed numeric transport, never evaluates physics or
parses user MOC/PNG/Physics3. SHA-256 uses the existing Windows CNG platform; no
third-party dependency. No model/derived state/digest is exported to public docs.
This is exact byte identity/custody, not model trust or author equivalence.

All original limits remain:512 MiB job/process, CPU2s, wall5s, total output192 MiB,
manual bindings32, logical steps1..8, clock<=1s, at most8 fixed steps of backlog.
Checkpoint transport cap128 KiB, chains<=128, total particles<=2048. No public ABI,
SDK capability, activation, merge, private-asset redistribution or GPU reset.

## Transport

Checkpoint `ARSPHYS1`: u32 version1, u32 chain count, u64 generation, u64 clock,
u64 remainder,32-byte package identity; each chain u32 count then particles with
four little-endian f64 values(position X/Y, velocity X/Y). Header72 bytes; exact
EOF, finite abs<=1e6. Identity = SHA256(u32 entry length || entry UTF-8 || ZIP bytes).

Worker `--step`: existing ARSMPK01 envelope, then one command with u64 logical
sequence, u64 clock, u32 controls0..3, u32 binding count, slot/f32 pairs, u32
predecessor length and exact predecessor bytes. First request sequence1/clock0
requires empty predecessor; later sequence/clock must follow restored state.

Candidate `ARSCAND1`: u32 version1,u32 reserved0,u64 nonce,u64 sequence,u64 clock,
u64 drawable length,u32 checkpoint length,u32 reserved0,32-byte package identity,
32-byte predecessor digest (SHA256(empty) for first step). Header120 bytes, then
ARSDRAW1 version4 generation=logical sequence+1 with physics flags, then checkpoint.
New version4 uses logical checkpoint generation; legacy v1/v2/v3 stay unchanged.

Acceptance order: typed full validation -> GPU candidate/exact query -> final
core fence -> checkpoint pointer/cursor assignment -> target swap -> observation.
Worker ACK only proves the restored predecessor in the inspected method; it
cannot authorize publication or turn hostile numeric output into trusted policy.

## Plan, authority and evidence

Add typed codec/hash and solver export/restore; opt-in step worker/broker; reuse
packet publication with checkpoint-bound request and owned fence; generated tests
and local resumed model proof; regress prior modes, review and document/commit.
Implementation is authorized by the current continuation; internal method choices
do not alter the full product criteria. Stop on unsupported actual results rather
than enlarging limits. All mandatory CP IDs need direct proof before stage PASS.

Reconciliation: README/ledger CURRENT_STATE, pipeline PLANNED target, PUB-04-A
contract/evidence historical bounded proof. Those pins and private dependency
viewer edits remain untouched. Full persistent session, SDK/qualification and
physical D06/D08 HOLDs are not waived by this stage.
