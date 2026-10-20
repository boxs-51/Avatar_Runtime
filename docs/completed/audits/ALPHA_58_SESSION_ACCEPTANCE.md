# Alpha #58: bounded isolated logical-session acceptance

2026-10-20, Asia/Bangkok. Base `3846f1f7ae19e7d163b316c2d192252b199c6be7`.
[Issue58](https://github.com/boxs-51/Avatar_Runtime/issues/58),
[R0 design](../../in-progress/ALPHA_58_SESSION_LIFECYCLE_DESIGN.md),
[wire and private API contract](../../in-progress/ALPHA_58_LEASE_PROTOCOL.md).

## Implemented behavior and scope

New opt-in `--lease` retains one admitted model/atlas/native solver per isolated
AppContainer lease. Versioned duplex messages distinguish prepared and accepted
solver state. One native publication owner independently validates a complete
candidate, binds generation to its actual CoreConsumer, waits for the exact GPU
query, checks cancellation/expiry/source authority, then publishes core and
immutable checkpoint/frame without a fallible post-core step. Worker ACCEPT is
sent only afterward. A lost ACK reaps the worker and restores that already
published checkpoint in a fresh epoch. Rejected speculative state never becomes
the next baseline. Native close permanently denies input before query drain.

Logical owner sessions outlive short leases through explicit native solver
restoration/rebasing; the host never rewrites particle or checkpoint time bytes.
Package replacement admits a fresh default/reset candidate while retaining the
old accepted frame/state until success. Failed admission, GPU target or final
fence preserves the old package; cross-package restore and stale output/controls
are denied. The private loopback API and mapped Tk viewer share this exact
isolated/native path. Trusted-local evaluator use is confined to independent
verification oracles, and is absent from the viewer/session implementation.

Legacy package/evaluate/simulate/checkpoint modes retain their old decoders and
bounds. The lease is also bounded: one active child,512MiB process/job memory,
2s user CPU,5s wall lifetime,192MiB cumulative output; one pending candidate,
32 bindings, local8 commands/1s,128KiB checkpoint,128 chains/2048 particles,
8-step backlog. No public capability/ABI or production host is enabled.

## Direct local acceptance

| Slice | Observed proof | Scoped verdict |
| --- | --- | --- |
| R1 | Native typed codec677 negatives, native epoch solver continuity; three real commands/ACCEPTs within one AppContainer lease; memory/CPU/wall/output/unread-pipe limits and actual profile/job cleanup | PASS |
| R2 | Both WARP/hardware: malformed output,target failure,cancel,late exact-query cancel,expiry and late source revoke preserve exact core/checkpoint/frame; recoverable failures retry in fresh epochs; revoke never regrants authority | PASS |
| R3 | Both backends: held input,complete omission/release,explicit reset,17 driven updates beyond1s logical time across multiple workers; actual owned-child crash,wall timeout,query rejection and lost ACK; accepted particles/remainder match continuous independent Python solver (<1e-6),full native pixels within1 byte | PASS |
| R4 | Both backends: two local packages with distinct atlas pixels; failed replacement retains old package; successful replacement/default/reset matches independent reference; delayed old candidate rejected before GPU; old package input and cross-package checkpoints rejected | PASS |
| R5 | Both backends:12 accepted API frames in a mapped window,widget pixels sampled against native readback; full API image within1 byte of independent continuous solver/evaluator reference; stale/invalid/unauthorized controls denied,actual revoke blocks update and preserves image,close cleans jobs/native/Tk | PASS |

Public generated negatives assert exact particle/vector shapes and finite errors,
exact PPM body length, invalid control schemas/CAS/deadlines, idle/partial/delayed-body HTTP
connections, atomic close admission, valid request and bounded handler shutdown. Eight tests PASS; independent
review ran these too. R1 codec and existing checkpoint/retained/drawable modes
passed their targeted regression. Full configured regression and immutable final
run are recorded below when complete; earlier passing runs do not substitute.

Private evidence and derivative assets stay ignored under `build/`; no model IDs,
asset data,geometry,image hashes or per-model metrics are published. Coarse PASS
and procedure counts above describe tests, not redistribution rights.

Reproduce with `python scripts/moc3_verify_session_acceptance.py` after building
`build/moc3-public-isolation` and `build/m2-d-private` Release diagnostics and
supplying the already-authorized local fixture. It runs same-lease and stale
ACCEPT probes,lease caps,R2-R4 integration and R5 mapped/API proof. The outer
local launcher snapshots the full public source/binary closure before Python
imports and after exit; the inner receipt independently checks before/after.
Structured receipts/logs live under `build/alpha58-final-acceptance/`.

## Review, reconciliation and final gate

Independent review found and resolved: initial generation could skip trusted
base; unread stdout could block broker join; BUSY close did not latch admission;
particle and PPM comparison could silently truncate through zip; unauthenticated
idle HTTP could block teardown. Generated regression and direct local observations
cover these corrections without relaxing assertions or caps. Final source/oracle
review reports no remaining supported P0-P2; architecture/project SUPPORTED,
optimization NO_RECOMMENDATION. Reviewer runtime receipt and final verification
are reconciled at delivery. Review independence does not claim a distinct GitHub
actor approval.

Final direct run: **PASS, exit0**, `build/alpha58-final-closure.log`.
Both `build/alpha58-final-acceptance/outer-closure.json` and `evidence.json`
record unchanged source/binary/dependency hashes; the outer snapshot precedes
session-module imports. Per-script logs in that folder record retained/stale-ACK,
caps,R2/R3/R4 and R5 completion. The final frontend regression passes all eight
generated checks (`build/alpha58-final-http-shutdown.log`). Full configured native
CTest:69/69 PASS (`build/alpha58-final-ctest.log`); the final mailbox-only delta
has its targeted frontend rerun above. Full contract suite:178/178 PASS,exit0
(`build/alpha58-final-contracts.log`). Local execution is primary; no remote CI
success is inferred. Catalog153 documents and347 resolved relative links.

The first unrestricted frontend regression exposed unfinished daemon-handler
logging during interpreter shutdown despite passing assertions. Handler joins,
atomic admission/waiter shutdown and the delayed-body negative corrected that
failure. The first restricted contract run hit Windows temp ACL denial; elevated
rerun passes unchanged assertions. Neither failed run is treated as acceptance.

G1 behavior,G2 artifacts,G3 direct integration/negative evidence,G4 unchanged
constraints,G5 attributable local closure,G6 separate review and G7 scoped closure
apply only to issue58. Exact commit/PR/alpha merge SHA is recorded on the issue;
classification as a completed document does not grant merge/release approval.
Main remains `8723ef174a7db98b05fd031aa39dbd910e5ba2db`.

## Remaining limits

This is a private diagnostic logical-session path, not a public SDK, production
context barrier or realtime performance qualification. The local atlas fixture
uses512px derivatives; arbitrary/full-resolution model support is not established.
Independent physics makes no Cubism/author fidelity claim. #59/#60 public ABI and
CPU lifecycle,#38/#39 production integration,#41/#42/#61 qualification and #62
release retain their separate gates. Physical D06/D08 remains HOLD without a
disposable runner; no reset/TDR,SDK use,private-asset publication,main merge or
public activation is performed.
