# PUB-04-B: bounded checkpoint restore and acknowledgement

2026-10-19, Asia/Bangkok. Scoped implementation against
[CP-01..04 C0](../../in-progress/M2_MOC3_PUB04_B_CHECKPOINT_DESIGN.md), baseline
`c925bb0668f5f6e3d04330dd2dd6342717000b3d`. Final verification and independent
review are recorded below. Full persistent PUB-02, full PUB-04 and shipping
activation remain **HOLD**.

## Actual transaction

Each fresh AppContainer worker restores the accepted solver particle positions,
velocities, clock and fixed-step remainder. It validates package identity and
actual solver shape before evaluation, ACKs the SHA-256 digest of the exact
predecessor bytes, and returns one typed candidate checkpoint plus drawable.
SHA-256 binds the exact ZIP bytes and manifest entry; it is identity/custody,
not authentication of worker fidelity or author-equivalent physics.

The trusted host independently validates all framing, counts, finite values,
sequence/clock, package and predecessor identity before GPU allocation. A pending
query pins immutable checkpoint and drawable data. Exact query completion then
the existing final CoreConsumer authorization precede nonthrowing checkpoint,
cursor and target acceptance. There is no fallible post-publication worker IPC.
Failed prepared sessions remain terminal; rejection retains the exact accepted
checkpoint bytes, core generation/value, target, epoch, cursor and pixels.

This is a bounded bridge: logical sequences1..8, clock<=1s, backlog<=8 fixed
steps, bindings<=32. Every job repeats package parsing and exits; the five-second
wall, two-second CPU,512MiB memory and192MiB output caps are unchanged. Checkpoint
transport is capped at128KiB,128 chains and2048 particles with finite bounded
doubles. A logical drawable generation is transport metadata, not a retained
in-process MOC evaluator. Persistent input ownership/expiry/release and whole-
package replacement still need a reviewed session resource policy.

## Direct requirement and task evidence

| Task / requirement | Direct witness | Result |
| --- | --- | --- |
| CP-T01 / CP-01 | Generated multichain solver restored fresh per step: exact parameters and serialized particles/velocity/clock/remainder match continuous solver across fractional-step remainder, OFF and reset | PASS |
| CP-T01 / CP-01 actual model | Eight fresh isolated jobs per backend restore accepted state; actual particle/velocity bytes change and OFF/reset restores rest; every accepted WARP/hardware image exactly matches same-backend continuous isolated reference | PASS |
| CP-T02 / CP-02 | Native wire corpus rejects truncation, wrong identities/version/reserved fields, nonfinite/count violations and malformed candidate; failed decode preserves output, incompatible actual solver shape leaves solver fresh | PASS |
| CP-T02 / CP-02 host boundary | Generated actual core/GPU tests reject invalid ACK, model, NaN and trailing bytes before new GPU creation; legacy batch/step mixing is denied | PASS |
| CP-T03 / CP-03 | Exact checkpoint remains unchanged before fence completion and changes with successful publication; wrong-thread APIs deny access | PASS |
| CP-T03 / CP-03 real failures | Both backends: bad ACK, malformed state, actual crash, wrong package, stale checkpoint, cancellation, elapsed expiry and query-ready revoke retain exact previous checkpoint/core/image; later grant denied | PASS |
| CP-T04 / CP-04 observation | Mapped window plus independent authenticated API client observe all eight accepted identities per backend, matching image/sample identities,401 denial and descriptor cleanup | PASS |
| CP-T04 / CP-04 regression | Integrated66/66 and isolated32/32 CTest, generated publication5/5, frontend2/2, native numeric/wire741 rejection cases; final source/binary/dependency closure captured before/after | PASS |

The authorized private model uses an explicit512-edge atlas derivative with
original MOC and Physics3 unchanged. This is independent-method evidence, not
original-resolution, universal MOC3 compatibility, throughput, shipping ABI or
Cubism/author-equivalence qualification. Assets, IDs, geometry, image hashes and
model-derived measurements remain ignored locally. No public asset redistribution,
new dependency download, public capability, merge, activation or GPU reset occurs.
Windows CNG BCrypt supplies SHA-256 as an existing platform facility.

## Commands, revision and local evidence

* `cmake --build build/m2-d-private --config Release` and
  `ctest --test-dir build/m2-d-private -C Release --output-on-failure`:
  **66/66 PASS**, `build/pub04b-final-build.log`, `build/pub04b-final-ctest.log`.
* `cmake --build build/moc3-public-isolation --config Release` and
  `ctest --test-dir build/moc3-public-isolation -C Release --output-on-failure`:
  **32/32 PASS**, `build/pub04b-isolation-full-build.log`,
  `build/pub04b-isolation-ctest.log`.
* `python scripts/moc3_verify_admission_observer.py --checkpoint`:
  **PASS**, `build/moc3-package-evidence/68bdf1b16b4f45c4b3a86898/`.
* `python scripts/moc3_verify_admission_observer.py --publication`:
  **PASS**, `build/moc3-package-evidence/2ac740fd29654df69f37f256/`.
* `python scripts/moc3_verify_public_preflight.py`: **PASS**,
  `build/f4-public-preflight-evidence/51ec2efbf4af4213b4e03d74/`,
  coarse log `build/pub04b-preflight.log`. Actual memory/CPU/wall caps,
  crash, output flood, file/process/environment/limited-loopback negatives and
  cleanup are observed. Universal network attribution stays HOLD.

Both final visible proofs have identical child before/after and separate
parent-before-creation/after-exit captures matching the exported current closure.
Every backend's eight accepted sequences was observed by the separate API client;
the UI mapping/sample checks agree and transient descriptors were removed.
The earlier build gap did not affect the isolated solver sources; the final
owner-thread change and rebuilt fence test belong to the integrated build.

Local Windows x64 MSVC Release with static-CRT AppContainer worker and Python3.12
is primary evidence. CI was not rerun and no GitHub gate or merge is claimed.
The [public-safe source closure](../../reference/audits/M2_MOC3_PUB04_B_SOURCE.json)
contains code/binary/dependency custody only. The
[separate review receipt](M2_MOC3_PUB04_B_REVIEW.md) identifies independent work.

An initial full build regenerated CMake late during project traversal, leaving
the publication host/fence binary older than the final owner-thread change.
The earlier66/66 and live checkpoint/publication runs are retained under
`build/pub04b-ctest.log`, `5ade0d2718b9429d997e1cd0` and
`f958d2d7eee8422c8433468a`. They are historical observations and **not** the final
closure. A complete rebuild and rerun with the final binaries resolves that gap;
no assertion, deadline, model resolution or production limit was relaxed.

## Reconciliation and completion

PDRM-01..04: README/ledger CURRENT_STATE, public pipeline PLANNED, PUB-04-A and
PUB-02-B reports historical bounded evidence, new C0 current scoped contract.
No document supplies merge/release authority. PDRM-05..07: README, ledger and
pipeline now distinguish actual restorable checkpoint ACK from the earlier host
cursor and from the unfinished persistent worker. PDRM-08..11: optional Windows
diagnostic addition; no migration/cutover/public ABI change. Existing batch modes
remain regressed. PDRM-12..16: exact closure, independent review, current catalog
and valid links accompany delivery; historical reports/pins remain unchanged.
Touched current documents: NO_DRIFT. Older scoped reports: INTENTIONAL_DIFFERENCE.
Shared-database MARM: NOT_APPLICABLE.

Completion contract C0: GOAL-CP is a CODE_CHANGE delivering actual bounded
checkpoint restore/ACK and atomic final host acceptance. CP-T01..04 map to
CP-01..04 in the direct matrix above at the current closure revision. Each task
is VERIFIED with DIRECT native/OS/GPU/API evidence. Independent review executed
its own targeted checks and found no supported open P0-P2; see its receipt.

G1 goal fidelity: PASS for the stated bounded bridge; G2 artifacts: codec,
solver/worker/host integration, tests and accessible evidence/review delivered;
G3 functional outcome: restored solver and real two-backend failures observed;
G4 constraints: all caps, private-data limits and unsupported SDK/physical gates
preserved; G5 evidence: final exact revision/pins and commands above; G6 review:
separate tests and source reconciliation PASS; G7 closure: current docs/catalog
and requirement coverage agree. Scoped code task, document task and GOAL-CP:
**PASS**. Whole public product goal: **UNVERIFIED**, with the remaining decisions
below held explicitly. Earlier M2-D custody/stack or sanitizer failures are not
waived by these separate tests. Commit/push is within the owner's continuing
instruction; merge/activation is not part of this delivery.

Remaining: persistent session/input ownership and expiry/release, package
replacement/lifecycle, installed public SDK and ABI, cross-platform, sanitizers,
fuzzing, quality-tier performance and rights-cleared fixtures. Physical D06/D08
remain HOLD until a disposable reset-capable runner exists. Healthy GPU and
injected failures here do not supply physical loss/recovery evidence. See the
[release ledger](../../in-progress/M2_MOC3_F4_RELEASE_LEDGER.md).
