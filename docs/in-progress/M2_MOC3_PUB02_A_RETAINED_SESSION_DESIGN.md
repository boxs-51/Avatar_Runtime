# PUB-02-A bounded retained evaluator sequence

Owner authorization: reorganize the documentation, then continue the unfinished
implementation. This is the first executable PUB-02 slice, after static PUB-01/03.
It does not close full PUB-02 or PUB-04. No public ABI/capability change.

## Frozen scope

* RET-01: one actual AppContainer admission owns one native evaluator and the
  admitted atlas set across 1..8 complete parameter frames. Decode the package
  once, then call the same Session with complete overrides; omitted bindings
  reset to the evaluator defaults. Reject duplicate/out-of-range/nonfinite input.
* RET-02: the trusted caller supplies sequence and relative clock. Sequence must
  be exactly 1..N; first timestamp is zero, later timestamps strictly increase,
  no delta exceeds eight 16,666,667 ns steps and total timestamp is at most one
  second. These are diagnostic clocks, not an authorization lease or physics clock.
* RET-03: versioned output carries every expected sequence, timestamp, request
  nonce and native snapshot generation. Independent native host validates the
  entire sequence and every drawable/atlas packet before any GPU creation. A
  rejected batch cannot advertise a partial candidate or change previous output.
* RET-04: demonstrate default → driven → released geometry/pixels on WARP and
  hardware, exact same-backend trusted-local comparison, negative input/output,
  request replay/staleness, process cleanup and a mapped window/API observation.

The first slice is a **bounded batch within one retained worker**, not interactive
duplex streaming or a long-lived public control service. The existing worker
limits remain 512 MiB, two CPU seconds, five wall seconds; aggregate output stays
192 MiB. No limit is raised to make a private fixture pass. The caller must not
reuse static preflight success to parse public bytes in the trusted renderer.

Linked Physics3 is still admitted and catalog-validated, **not simulated** by this
slice. Parameter motion must never be labelled physics. Persistent isolated
physics state, prepare/commit, expiry/cancellation/reload and late core revoke
remain subsequent PUB-02/PUB-04 requirements. The original full-resolution model
can still exceed the profile; successful derivative fixtures remain test-only.

## Protocol

Worker mode `--evaluate` accepts the existing bounded `ARSMPK01` ZIP envelope,
followed by u32 frame count. Each frame: u64 sequence, u64 timestamp_ns, u32
reserved=0, u32 binding count<=32, followed by (u32 catalog slot, f32 normalized
value in [-1,1]) pairs. Catalog slot is local to this admission; IDs never cross
the worker boundary. Values map from default towards the checked minimum/maximum.
The full input has exact EOF and all commands validate before output.

Output: `ARSEVAL1`, u32 version=1, u32 count, u64 nonce, u64 total length (32-byte
header). Each record: u64 sequence, u64 timestamp_ns, u64 drawable packet length,
then `ARSDRAW1` version=2. Inner packet layout matches static version=1, except
snapshot generation is sequence+1. Static v1 decoders continue to reject v2.
Both outer and inner framing use explicit little endian fields, exact EOF,
checked sums, fixed count limits and no filenames/pointers/shaders/host handles.

The native sequence decoder first validates all outer records and all inner
packets without scene allocation, then constructs immutable verified frames.
Failure leaves its previous verified sequence unchanged. The packet-only GPU
host may select an expected record for diagnostic rendering after this complete
validation; it never evaluates the model. Product core publication is not inferred
from this host query/readback transaction.
