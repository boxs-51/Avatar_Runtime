# Alpha58 lease protocol v2, R1 frozen wire contract

2026-10-20. Implements the R0 logical-session direction in
[session design](ALPHA_58_SESSION_LIFECYCLE_DESIGN.md). New opt-in `--lease` only;
legacy modes/decoders stay bounded and unchanged. No public ABI/SDK activation.
All integers little endian; exact frame length, reserved zero, finite floats.
Per job512MiB/userCPU2s/wall5s/output192MiB still cumulative across all commands.

Package startup: original ARSMPK01 envelope followed by session:u64,
package_revision:u64,epoch:u64,base_generation:u64, restore_length:u32, restore_bytes<=128KiB.
All three identities nonzero. Worker admits/decode package once, retains one
evaluator/atlas/solver and rejects unsupported input before READY.

Duplex frame: magic8 `ARSLES02`, version:u32=2, kind:u32, body_length:u64.
Each body<=192MiB-24; cumulative emitted bytes<=192MiB. Kinds:1 READY(out),
2 PREPARE(in),3 CANDIDATE(out),4 ACCEPT(in),5 ACCEPTED(out),6 CLOSE(in),7 CLOSED(out).
Unexpected kind/state/duplicate, truncation or trailing body bytes terminate lease.
READY body: model_digest32, accepted_checkpoint_digest32, session/package/epoch
threeu64,base_generation:u64, catalog_count:u32<=4096, catalog bounds(min/default/max)threef32 each.
Empty predecessor and READY digest are SHA256(empty), never an all-zero sentinel.
A restored checkpoint must have epoch strictly less than startup epoch and exact
session/package_revision/model lineage; equal/future/cross-lineage restoration is
rejected. Initial package generation/clock is host-bound: a fresh package may
start at host-authorized base_generation+1 during replacement, but package clock
starts0. With the default base_generation0, first generation MUST be1. The native
publication owner supplies base_generation from its actual accepted core state;
worker-supplied values can never choose it. Overflow is rejected.
IDs never leave the isolated evaluator. Catalog slots belong to that package.

Key = nineu64(session,package_revision,epoch,global_generation,global_clock_ns,
local_sequence,local_clock_ns,control_revision,nonce) + model_digest32 +
accepted_predecessor_digest32 =136bytes. Nonzero identities/nonce/generation;
global_generation<UINT64_MAX; control_revision=global_generation for complete
manual snapshots; local_sequence1..8/local_clock<=1s. Global time/sequence are
logical metadata, not resource/backlog permissions. Within a lease local clock
increments by exact global delta. At new epoch local_sequence1/local_clock=global
delta since accepted predecessor (zero for initial). Global generation is exactly
accepted+1. Global time strictly increases after initial0; delta+remainder<=8 fixed
16,666,667ns steps. Epoch increments on each lease, even if no state was accepted;
host can skip dead epochs but cannot reuse them. Package replacement requires
new package_revision and empty predecessor; accepted old package stays visible
until new default/reset state commits.

PREPARE body: Key,control:u32(bits enabled1/reset2),binding_count:u32<=32,
(slot:u32,value:f32 normalized[-1,1]) pairs. Unique slots, complete overrides;
omission returns model defaults. Exactly one pending candidate; no subsequent
PREPARE until exact ACCEPT. Rejection/cancel/expiry/revoke kills/reaps lease.

Checkpoint: magic8 `ARSPHY02`,Key,inner_length:u32,reserved:u32=0, inner ARSPHYS1.
Entire checkpoint<=128KiB. Inner bounded generation/clock exactly local key.
Particles/velocity/remainder remain legacy numeric bounds/shape. Restoration is
explicit inside the isolated solver: validate origin/session/package/digest,
copy accepted particles/remainder, rebase native solver to new epoch generation0/
clock0, then advance by requested delta. Host never edits accepted state bytes.

Candidate: magic8 `ARSCND02`,version:u32=2,reserved:u32=0,total_length:u64,
Key,frame_length:u64,checkpoint_length:u32,reserved:u32=0,frame,checkpoint.
Frame uses opt-in ARSDRAW1 version5/global_generation+1; old versions retain their
existing generation<=9 checks. Host validates complete candidate/key/checkpoint/
frame before scene allocation/GPU. Checkpoint Key must exactly match outer Key.

ACCEPT and ACCEPTED body: exact Key + SHA256(checkpoint bytes). Worker commits
its prepared solver candidate only on that exact trusted-host message. Host sends
it only after final core/frame/checkpoint publication. Lost ACCEPT/ACK cannot
roll back published state; terminate/reap and restore the authoritative accepted
checkpoint in a new epoch. Worker output never authenticates host authority.
CLOSE/CLOSED bodies empty; pending state may only be discarded by terminating.

R1 freezes encoding/transition oracle and actual retained lease behavior. R2 adds
host atomic publication, R3 rollover/crash recovery, R4 package/owner replacement,
R5 mapped agent API. Each stage needs its design acceptance; static wire proof
alone cannot close #58. Detailed model/atlas/particle results stay local ignored.


## Private owner/API adapter

`scripts/moc3_isolated_session.py` owns one native LeasePublication and at most
one isolated worker. It rotates after eight local commands, a dead child or
three seconds of worker age; five-second wall/two-second user-CPU lifetime caps
remain authoritative. It restores only actual native host-accepted bytes.
A rejected lease is reaped before retry; real source revocation is permanent.
Closing native command admission latches before any BUSY query drain, and
readback is denied once native closing starts. No public context ABI is implied.
The native diagnostic core uses 32 ordinal manual input channels; worker catalog
slots select model parameters independently. This is not a public parameter ABI.

`scripts/moc3_session_viewer.py` serializes native, lease and Tk operations on the
owner thread. Its private API binds only `127.0.0.1` with a random port and bearer
secret saved in ignored `agent-endpoint.json`; it sets no CORS permission. HTTP
connections have a one-second pre-header socket timeout, at most eight handlers,
16KiB request bodies, eight queued commands and a two-second owner admission
deadline established before queueing. Shutdown seals command admission under the mailbox lock before teardown,
closes tracked sockets, releases every registered waiter and joins handlers
within a shared two-second bound; an unfinished join is reported UNVERIFIED. Slow or
unauthenticated connections cannot hold viewer shutdown. The deadline is never
renewed by worker admission; the native final fence independently enforces it.

POST `/status` and `/frame` take exactly `{}`. Frame adds a complete base64 PPM
of the accepted native readback; no alternate renderer supplies API pixels.
POST `/update` requires exactly `session`, `package`, `generation`, `sequence`,
`bindings`, `control`, `advance_ns`. First four are exact integers matching
current session/package/accepted generation/next owner request sequence. Bindings
are complete `[slot, normalized_value]` arrays under the wire limits. Control is
0=off,1=on,2=off+reset,3=on+reset. Explicit logical advance is 1..8 fixed steps;
first/package-default candidate has clock0. This logical clock is independent
of wall presentation; no realtime FPS qualification is claimed. Schema/type/
range/freshness validation precedes work. A valid failed update consumes its
request sequence; accepted generation changes only on actual publication.
POST `/revoke` and `/close` take only the four identity/sequence fields. Revoke
calls the actual native source authority and retires the worker; later updates
remain denied while accepted readback remains available. Close reaps the job,
drains/closes the native owner and destroys Tk resources on their creating
thread. Endpoint shutdown rejects subsequent connections.

No API endpoint uploads ZIPs, reads model paths, exposes IDs or edits checkpoints.
Replacement is the trusted owner's direct method, tested with two permitted
local packages. Public ABI/import/distribution are separate #59/#60 work.
To observe locally, run `python scripts/moc3_session_viewer.py`; the authorized
fixture is discovered locally and derivative assets/evidence stay under build/.
