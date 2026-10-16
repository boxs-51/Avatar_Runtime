# M1-C preparation: bounded CPU asset validation

State: PREPARATION_ONLY. Implementation and merge are not authorized by this
document. Baseline main: `7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79`.
M1-B CPU scope is integrated; overall B security remains HOLD as documented in
`M1_B_COMPLETION_REVIEW.md`. This dependency handoff does not close B.

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
No parser execution, dependency installation or production implementation is
claimed. Those concrete choices must be accepted before C1 begins; C0 approval
does not automatically authorize C1/C2 or their merges.

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
