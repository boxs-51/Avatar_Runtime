# M1-C acceptance proposal

State: OWNER_PLAN_APPROVED_C0_C1_C2; C0_HOLD_SCHEMA_ALLOCATION_BOUNDARY.
No G02/G08 PASS is conferred here. The implementation plan is approved, but its
dependency gate must pass before C1 starts.

## Executed C0 dependency gate

- Four upstream source archives pinned and hashed; full local source inventory
  retained with its digest in `M1_C_DEPENDENCY_LOCK.json`.
- zlib/libpng/utf8proc local native allocator probes PASS, including actual MSVC
  ASAN-instrumented decoder objects and allocation failure cleanup. This is not
  the C1 platform matrix. Corrected harness failures remain disclosed in the lock.
- jsoncons PMR schema gate **RED**, exit 1: 36 allocations / 1664 requested bytes
  outside the fixed arena on a valid instance. In-process schema acceptance fails.
- Forty fixture payloads generated twice with identical digests; three positive
  schema instances checked. Native corpus execution is still NOT_EXECUTED.
- C0 complete dependency acceptance remains HOLD; C1/C2 have not started. A
  concrete seven-path helper-process amendment is proposed for owner decision,
  or an in-process replacement must pass the original hard-allocation gate.

## Required native implementation gates (not yet executed)

| Gate | Required evidence before acceptance |
| --- | --- |
| C0 dependency lock | Exact selected source/version/hash/license, primary-source links, transitive inventory, bounded allocator/depth strategy, offline clean configure, security review date and unresolved dependency risks |
| C0 fixtures | Reproducible original/generated PNG/GLB/ARSCHAR assets; license, seed/tool command, SHA256, expected status and allocation ceiling per case; missing examples are not accepted bundles |
| C1 archive | Traversal/UNC/drive/device/symlink, invalid encoding and Unicode/case aliases, duplicate/local-central conflicts, overlap/truncation/CRC/compression/encryption/ZIP64 policy, nested bombs, real streaming inflated/entry/byte ceilings |
| C1 strict JSON/schema | Duplicate keys, truncated/invalid UTF-8, depth/work limits, true distinct from 1, NaN/Inf/exponent overflow, schema metaschema and reference behavior, schema parity against actual Draft 2020-12 reference validator |
| C1 semantics | All FMT-001..018 applicable oracles; missing/duplicate semantic/layer/part IDs, type/unit/default errors, dangling bindings, parent+mask union cycles, required mode absent, BAD_ASSET precedence over disabled features |
| C1 actual PNG | Validate chunk lengths/order/CRC and actual decode; truncated/oversized/decompression failures, pixel/stride arithmetic and aggregate RAM; atlas rectangles use actual image bounds rather than canvas |
| C1 positive asset | Genuine complete 2D Profile B bundle, finite vertices/UV/indices/deltas, immutable CPU result; zero GPU/network/file extraction and balanced host allocations on every failure |
| C2 GLB | Header/chunk alignment/length, integer overflow, view/accessor stride/count/component alignment and bounds, actual indices, sparse policy, skins/joints/weights, node cycles/transforms, morph/animation references and finite values, embedded images, external URI and required extensions |
| C2 integration | Genuine 3D and dual-mode CPU bundles, deterministic semantic catalog; valid disabled VRM1/mask/weighted features rejected, malformed equivalents BAD_ASSET; no public source activation |
| Faults/lifetime | Fail every allocator stage, cancellation/close where integrated, oversized hostile declarations, output rollback, no leaks/UAF or partial immutable publication; repeat success after a failed attempt |
| Fuzz | Seeded corpus, parser entry points, deterministic regression replay on Linux/Windows; libFuzzer+ASAN/UBSAN timed campaign with exact duration/exec count/RSS/timeouts/crashes/artifact hashes; any crash fixed and retained as regression before readiness |
| Build/ABI | Exact candidate SHA on GCC/Clang + MSVC x64 shared/static, real sanitizers inspected and executed, frozen ABI20 exports/layouts, both plain-C consumers, historical M0/A/B tests; TSan on actual shared asset/cache/lifetime paths if introduced |

No arbitrary duration is treated as proof of absence of bugs. C0 fixes a bounded
per-PR CI fuzz budget and longer campaign budget, reproducible seeds and timeout/
memory ceilings; exhausted runners/timeouts are incomplete gates, not success.

Negative-first proof must exercise compiled production parser behavior, with
source SHA, command, expected/actual result and exit code. Include mutation
controls omitting an expanded-byte cap, alias detection, semantic-before-feature
precedence, checked accessor bound, actual image dimensions and atomic result
publication. Do not substitute source-text/path failures for parser RED evidence.

Each executed case records ID, fixture hash, exact source/base/tree, compiler/OS,
flags, seed, command, expected/actual status, allocation/network/GPU counters,
stdout/stderr/exit and raw artifact digest. Aggregate totals alone do not clear
a missing case. Preserve every original B inventory record and verdict until
the complete original scenario actually runs; C results do not clear GPU,
animation/mode/adapter or public B integration automatically.

C0 is a decision packet; C1 is CPU 2D ingestion component acceptance; C2 is CPU
2D+GLB ingestion component acceptance. Public model integration and full G02/G08
clearance need their actual complete paths and separate evidence. Implementation,
merge, public activation and SDK release are distinct decisions.
