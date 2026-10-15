# M1-B2 CPU authority evidence and boundary

Owner approved **B2 only**, PR22 preparation source
`c56a9cd2d04dbab08aa5cc4eedf04ab4bd7efdf5`, its 33-path manifest and decision
table, by replying YES in the current conversation. Base main is
`535663960c33c0ebe49481a8681b8625c336fafe` (landed B1/PR21).
PR23 is the implementation. B3 and PR23 merge require separate decisions.
Final source SHA and matching CI runs belong in PR23 to avoid self-reference.
Classification: SELF_AUDIT / INDEPENDENCE_NOT_AVAILABLE.

## Production components and preservation

The real runtime lazily owns a bounded SourceAuthority context, allocated and
freed through its retained host allocator outside every internal lock. Bare
runtime creation does not allocate maximum source queues. A static size bound
and an injectable lower private byte budget enforce 8 MiB including runtime
metadata. There are no untracked per-ingress heap allocations.

An internal CPU-binding path validates/copies immutable semantic catalogs and
inserts typed character/source records into the existing HandleRegistry. It is
a future asset-validator integration seam, not a public dummy model loader.
Direct object and retained-frame leases include a runtime lease; record storage
cannot be recycled while older generation pins survive.

Implemented production components: QPC/BCrypt or CLOCK_MONOTONIC/getrandom,
checked exact nanosecond conversion, serialized runtime high-water and sticky
clock fault, append-only lifetime token reservation/eligibility, immutable source
grants, bounded copied frames/queues/workspaces, atomic queue-sequence-quota
admission, R5 sticky closing intent/CV drainage and fail-closed rotate/revoke/
terminal expiry/exhaustion. Linux entropy handles partial/EINTR reads with an
eight-call bound and GRND_NONBLOCK failure, without fallback. Windows uses
system-preferred BCryptGenRandom. Native smoke is not a statistical entropy proof.

Limits match the approved table: 16 source queue reservations, 64 source records,
16 CPU character bindings, 256 catalog/frame channels, eight admitted workers,
16 preparation workspaces, 16 queued frames/source, 4096 lifetime token entries,
eight mint attempts, 1..240 fps with one-second burst and TTL at most one hour.
Source records may fail allocation before simultaneous maxima if resources are
retained. ASCII IDs use the frozen grammar. Empty scope means deny-all. Rotation
does not extend expiry; failure never restores the old token or superseded ticket.
Linux security TTL uses CLOCK_MONOTONIC and does not promise suspend-inclusive
wall-clock expiry. Simulation/capture timestamps never authorize TTL or quota.

The admission mutex is held only for reader count/closing intent and is released
before source/clock locks. An atomic release/acquire close-intent observation
avoids taking this mutex from within authority gates. Finalizer counters include
ingress plus the query's own management lease; ordinary old CPU leases may still
unpin asynchronously after close. Terminal dirty tombstones are release-published;
B3's acquire/recompute/publish path is not implemented here.

Public ABI header, all ABI implementation files, exact-20 export oracle, frozen
87-case inventory and 9+8 design blobs are unchanged. Public source/model/
character/update/render APIs remain UNSUPPORTED and capabilities zero.
Foundation still runs all 189 historical behavior assertions. Six historical
B1 custody cases now bind to the exact landed B1 snapshot without changing their
oracles; the new B2 gate independently enforces exactly 33 paths and negative
checks for activation, historical dilution, mock export and fabricated PASS.

## RED and native evidence

RED commit `e01e2326cb837b3262779976b0c1a7c67ad30264`: compiled real MSVC C++
runtime contract creates the runtime then fails B2 private validated binding:
expected OK, actual UNSUPPORTED, exit 1. This uses a private unimplemented seam;
it does not expose an ABI symbol. Raw output:
`build/integration-evidence/b2-behavior-red.txt`.
Initial remote runs fail source custody before native B2 execution; they are
not sanitizer or race evidence. Final run links and source are recorded in PR23.

Local Windows MSVC 19.51 shared Debug has passed 16/16 CTest groups, including
the preserved foundation/PREP/B1 suites, both exact export checks, native OS
security, source behavior and the controlled race suite. Static/ASan and the
full exact-source CI matrix must also pass before the PR is marked ready.

The race suite records seed `0x4152534232330001` and 1000 iterations per grouped
family: G/sequence/OOM, E rollover, A/H unlocked copy versus revoke, K/L pending
mint versus terminal revoke/admin release/failure, U phase-B expiry, N/Q/S
runtime clock poison, W checked terminal counters, M close versus pending
activation and R/T eight admitted plus eight denied ingress workers. Schedules
use barriers, latches and CVs, not sleeps. Pose/mixer/GPU portions of original
race oracles remain HOLD. Each failure prints family, seed, iteration and status.

Native fault tests exercise the actual platform entropy fill loop with injected
partial/error results, the same clock arithmetic and poison implementation,
pending/retired token collisions, quota/queue rollback, callback reentry and
exception containment, source/byte/workspace budgets, retained-frame ownership,
host allocation failure/alignment and close reentry, plus exact phase-C ticket
comparison. Fixture/backend/counter definitions exist only in test translation
units; production has no reset/setter/mock export or token logging.

Raw CTest output is retained under `build/b2-local-*/Testing/Temporary/LastTest.log`.
CI retains all native logs and actual sanitizer instrumentation, and runs separate
plain-C foundation/core consumers for each configuration. A matrix declaration
alone is not proof of execution; see the final exact-source attestation in PR23.

## Remaining holds and rollback

All 87 original whole-case verdicts remain HOLD in M1_B_CASE_EXECUTION.json;
original records/oracles/platforms are preserved. No source C ABI activation,
host network adapter, asset parser, real mode switch, complete retarget/motion
engine, B3 publication, physical GPU completion, G07/G08 or complete SDK claim.
Same-account checks are not independent actor review. Owner merge approval is
separate; rollback after any later approved merge is a normal reviewed revert,
preserving B1 and all frozen history. No force-push or rule bypass is authorized.
