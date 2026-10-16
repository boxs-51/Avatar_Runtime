# M1-C preparation: bounded CPU asset validation

State: OWNER_PLAN_APPROVED_C0_C1_C2 / C0_HOLD_SCHEMA_ALLOCATION_BOUNDARY.
Owner instruction: `toi phe duyet ke hoach C0,1,2`. This supersedes the earlier
per-stage implementation-approval recommendations; all three plans are approved.
Merge and public activation remain separate. Baseline main:
`7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79`.
M1-B CPU scope is integrated; overall B security remains HOLD. Its completion
review is the historical [PR25 snapshot](https://github.com/boxs-51/Avatar_Runtime/blob/43beccb131f31ea8a7b54c7151250279bd11acee/docs/M1_B_COMPLETION_REVIEW.md),
not an unmerged production predecessor. This dependency handoff does not close B.

## C0 executed result and concrete blocker

`M1_C_DEPENDENCY_LOCK.json` pins four actual upstream commits, downloaded-archive
SHA256s, license/source hashes, exact limits and transitive dependencies.
`M1_C_FIXTURE_PLAN.json` embeds a reproducible generator and 40 actual fixture
hashes, plus the remaining required native-negative/fuzz obligations. Regeneration
produces identical hashes; three positive manifest/rig combinations pass the
actual Python Draft202012Validator. No native ARSCHAR/GLB parser has executed.

Pinned zlib 1.3.2, libpng 1.6.59 and utf8proc 2.12.0 pass local Windows native
allocation probes. PNG is decoded under actual ASAN and every allocation-failure
cut is cleaned up. Their full C1 Linux/Windows/fuzz gates remain outstanding.

Pinned jsoncons 1.10.0 supports Draft 2020-12, but its schema evaluator's ordinary
containers/regex allocate outside the PMR DOM arena. A compiled standalone probe
validates a simple instance yet records **36 global allocations / 1664 requested
bytes outside PMR**, exit 1 against the required zero-outside-allocation oracle.
Its exact code, commands and log hashes are embedded in the lock. The result
rejects this candidate for the promised in-process hard allocation boundary;
it is not a claim that the library violates its documented allocator contract.

C0 verdict is **HOLD_SCHEMA_ALLOCATION_BOUNDARY**. C1/C2 implementation approval
is retained, but the prerequisite dependency gate has not passed. Do not remove
the allocation oracle, substitute a partial schema checker, or call C1 ready.

Concrete proposed remediation: run schema validation only in an owned bounded
helper process, with immutable request <=1 MiB, reply <=128 bytes, one worker,
128 MiB process reservation and 5-second deadline. The parent still performs
strict bounded JSON and all semantic/geometry validation; no asset-directed
commands, binaries or network resolvers. Crash/timeout/invalid reply fails with
zero publication and complete process/pipe cleanup. Resource isolation does not
claim a full hostile-code sandbox. Windows Job Object and Linux process limits,
helper artifact deployment and IPC lifetime are new architecture obligations.

This is a material amendment, not already implemented. Seven exact additional
paths and failure/ownership rules are listed in
`M1_C_DEPENDENCY_LOCK.json.material_decision_required`; proposed C1 total becomes
41 paths. Owner may instead retain the original in-process scope and require a
new bounded library/allocator remediation to pass the same gate. No additional
paths are authorized by this proposal, and no helper process is shipped/launched
as production code. Local C0 probe executables only tested dependencies.

## Reuse and gaps

Reuse ABI20, runtime allocator/budgets/leases, typed statuses, source semantic
catalog validation, Profile B schemas and independent design fixtures. Preserve
the historical M0/M1-A/B custody tests and all 9+8 frozen design blobs.
Current Python schema/semantic fixtures are design oracles, not a native parser.
There is no src/assets, src/validation, real ZIP/PNG/GLB parser or fuzz corpus.
The sample JSON references absent binary assets and cannot be a positive bundle.
Public ars_model_load and character/source/update/render remain UNSUPPORTED.

Owner-approved canonical RC1 Profile B/ABI20 decisions supersede historical
pending-design wording in frozen documents; do not edit their pinned blobs to
modernize status. Production proof is still required. No new public symbol,
schema feature, network fetching, GPU upload, VRM1, weighted meshes or masks.

## Ordered stages and first decision

**C0 — dependency and corpus design gate (recommended first kickoff).** Produce
exact dependency versions, immutable upstream URLs/hashes, licenses, vulnerability
review dated at selection time, transitive source inventory and offline build
strategy for ZIP/deflate, strict JSON + Draft 2020-12, Unicode normalization/case
alias detection and PNG. Select the GLB parser strategy and extension subset.
Use primary upstream specifications/docs and inspect the actual chosen source.
Do not substitute a custom partial JSON-schema checker for Draft 2020-12.
Validate library allocation hooks/depth/iteration limits before accepting it.
If a dependency cannot be bounded, reject the choice or scope a separate process;
do not quietly exclude untrusted data from the threat model.

C0 also specifies reproducible generated fixtures, per-file SHA256 and license,
exact GLB accessor/skin/morph/animation subset, budget arithmetic and negative
oracles. It delivers a dependency lock proposal and fixture specification only.
No ARSCHAR/GLB parser execution or production implementation is claimed by C0.
Local isolated dependency probe builds are recorded separately above. The owner
has approved C1/C2 plans; only the newly identified material schema-boundary
amendment remains undecided. No stage's implementation approval authorizes merge.

**C1 — native CPU envelope, JSON, Profile B and PNG.** Owned bounded memory
input -> archive index -> strict JSON/schema -> semantic/cross-file graph ->
actual PNG decode -> immutable internal ValidatedCpuModel. Real minimal 2D
bundle required. A combined malformed/disabled request must fail BAD_ASSET
before UNSUPPORTED after safe budget preflight. A valid required 3D payload
cannot be silently skipped while C2 is absent; classify it UNSUPPORTED only
after safe structural validation. C1 is a 2D component result, not full G02.

**C2 — bounded GLB CPU subset and complete corpus.** Reuse C1 acquisition and
limits; validate actual GLB bytes, references and checked accessor arithmetic,
skin/node graphs, joint/weight/index bounds, finite transforms/quaternions,
morph and animation arrays and embedded images. Deny external URI/code and
unknown required capabilities. Real minimal 3D and dual-mode bundles plus
corruption/fuzz corpus are required. C2 closes the accepted CPU ingestion scope
only after its exact-source matrix passes; it does not implement rendering.

**Return to B/public integration — separate scope.** Derive a canonical immutable
semantic catalog from validated assets and bind it to actual runtime model and
character ownership. Add ABI model/character/source activation only through a
separate concrete proposal, with lifetime/close/allocator and original security
scenarios. Never export a test catalog or enable feature bits to bypass C.

## Proposed decisions and resource ceilings

All limits are checked before allocation/iteration and cumulatively during
streaming decode. Configurable limits can tighten ceilings, never raise them
silently. C0 must demonstrate a consistent total resident-memory accounting
model (input + inflated members + parser + decoded assets + scratch + publication).

| Policy | Proposal / source |
| --- | --- |
| Compressed input / expanded total / entries | <=256 MiB / <=512 MiB / <=4096, existing specification ceilings |
| JSON document / image edge / total vertices / joints / visual layers | <=1 MiB / <=8192 / <=2,000,000 / <=256 / <=128, existing specification ceilings |
| Decoder working memory and decoded pixel aggregate | Explicit host-accounted caps chosen in C0; per-edge checks alone are insufficient |
| JSON/container depth, path length, graph nodes, PNG chunks, GLB objects and work steps | Finite exact caps and overflow-safe formulas required in C0 before C1 implementation |
| Archive behavior | Memory-only access; no filesystem extraction, network, recursive archive ingestion or external references |
| Names | Strict encoding; reject traversal/absolute/drive/UNC/device/symlink/duplicates and normalization/case aliases. Normalize only for collision detection, never silently redirect references |
| ZIP envelope | Check local/central directory agreement, overlapping ranges, sizes, compression methods, encryption flags, CRC, truncated streams and trailing/ambiguous members; ZIP64 disposition fixed in C0 |
| Input lifetime | Copy or retain an explicit owned immutable buffer; no borrowed caller bytes after return |
| Publication | Publish only a complete immutable CPU object; failed parse leaves no partial registry entry and balances all allocations |

Frozen error taxonomy: INVALID_ARGUMENT for caller contract errors;
OUT_OF_BUDGET for resource ceilings; BAD_ASSET for malformed or unsafe bytes,
duplicate keys/names, invalid references and numeric/geometry errors;
UNSUPPORTED for semantically valid recognized disabled features; INTERNAL for
unexpected internal failure after cleanup. No exception crosses the C ABI.
Malformed input is not relabeled UNSUPPORTED to avoid validating it. Diagnostics
must be bounded and avoid raw payloads, bearer tokens or sensitive host paths.

## Integration and custody

C implementation starts from actual refreshed main, not an invented parent.
The manifest names exact proposed paths per stage. Dependency vendoring paths
are deliberately not pre-authorized: C0 must enumerate the actual chosen files
or immutable external-source mechanism before the C1 manifest is accepted.
No src/** wildcard grants or edits to frozen ABI/schema/spec blobs.

The current B3 custody tests deliberately bind HEAD to its 31-path scope. C1
must pin all nine B3 custody tests, including their mocked git argument targets,
to landed B3 `7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79`, preserve the original
check_m1_b3 algorithm, and add a separate C source/base/path gate. Do not broaden
the B3 allowlist, remove tests or claim a docs-only branch passes its source gate.

Exact-source Linux GCC/Clang and Windows MSVC shared/static, ASAN/UBSAN, applicable
TSan, export/layout/consumer regressions and deterministic corruption/fuzz
evidence are required by `M1_C_ACCEPTANCE_MATRIX.md`. Private test hooks stay out
of shipping objects. Same-account review is SELF_AUDIT /
INDEPENDENCE_NOT_AVAILABLE. Every implementation PR still needs an exact-head
merge approval. Rollback: close this preparation or normally revert an approved
implementation; preserve all prior evidence/history.
