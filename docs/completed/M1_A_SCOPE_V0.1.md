# M1-A scope: build and ABI foundation

Status: SCOPE_DEFINED / IMPLEMENTATION_NOT_STARTED. This document defines the
next increment requested by the owner; it does not activate M1, change ABI20,
certify G01/G10, or authorize merging its future implementation PR.

Design baseline: G00 owner-approved PR14
`fe3e680a44ac4e71ef57dc177e73ae31198fcbd0`; public header Git blob
`1c61da4a48c9faf2001fc98ad4c084413781e61c`. Integration candidate PR17 preserves
that header and the 9 predecessor + 8 G00 blob pins. At kickoff record the actual
landed main SHA, header blob and toolchain versions in the new M1-A Draft PR.

This scope refines the historical `docs/M1_SOURCE_BOOTSTRAP_PLAN.md` for M1-A.
The older bootstrap topology/path list is planning history; the exact manifest
below supplies the missing implementation and consumer source ownership. Frozen
G00 documents and public declarations remain the accepted design reference.

## Deliverables and exact source manifest

| Paths | Deliverable |
| --- | --- |
| `CMakeLists.txt` | Keep PREP targets; add a separate C++20 foundation target with explicit opt-in and clear stage naming |
| `src/abi/abi_version.cpp` | Actual ABI version export using the existing encoded major/minor constants |
| `src/abi/unsupported.cpp` | Explicit UNSUPPORTED entry points for APIs whose implementation belongs to later stages; deterministic documented failure outputs |
| `cmake/ars_compiler_options.cmake` | Target-scoped warnings as errors, C++20, hidden symbols on ELF and explicit public exports |
| `cmake/ars_verify_exports.cmake` | Platform export inspection against the exact 20 existing public function names; exclude legacy `_p0` and diagnostic #21 |
| `tests/abi/foundation_contract.c` | C11 linkage, ABI version, deterministic unsupported outputs and capability non-advertising checks |
| `tests/abi/foundation_cpp.cpp` | C++20 consumer linkage and fixed-width layouts, with no STL at the public boundary |
| `examples/plain_c/CMakeLists.txt` and `examples/plain_c/main.c` | Separate build-tree C consumer that links the new foundation target; no dependence on PREP stubs |
| `.github/workflows/m1-build.yml` | Linux GCC/Clang + Windows MSVC x64 configure/build/CTest and export evidence |
| `docs/audits/M1_A_BUILD_EVIDENCE.md` | Exact final SHA, commands, toolchains, test results, actual exports, negative cases and remaining HOLDs |
| `tests/contracts/check_m0_rc1_pins.py` and `tests/contracts/test_m1_a_custody.py` | Separate, reviewed M1-A source/path transition based on the actual landed main; preserve historical M0 oracles and frozen design blobs |

`include/ars/ars.h` is consumed unchanged. No schema, security contract, feature
bit, status value, signature or ownership change belongs to this increment.
The next PR must name the paths it actually changes within this manifest; adding
paths requires a scoped decision and review before implementation.

## Behavioral boundary

The foundation is a real C++20 build target, not a working character runtime.
`ars_get_abi_version` returns the accepted ABI version. All other APIs remain
explicitly UNSUPPORTED without allocating a runtime, issuing a valid handle or
capability, advertising implemented features, invoking host callbacks, parsing
assets, or touching a graphics device. Do not reinterpret the existing PREP
test library as the runtime or copy its identity into a shipping package.

Follow the public contract for caller-owned buffers: read no suffix before
validating a readable size/version prefix; never exceed caller-declared writable
capacity; zero documented handle/cap outputs on failure. Tests must include
null, short/exact/long buffers, incompatible major versions, unknown minor
suffixes, nonzero reserved fields and canaries after output capacity. Do not
probe deliberately inaccessible pointers. Ambiguous error precedence or output
semantics must be resolved against the accepted ABI contract before coding.

Runtime create/close success requires a durable ID registry and lifetime rules;
that is M1-B. Keep it UNSUPPORTED here. This avoids introducing a temporary raw
pointer registry, counter reuse, or lifecycle semantics needing later replacement.

## Acceptance and execution order

1. Record landed M0 main/header identities, confirm this source manifest and
   receive explicit M1-A kickoff. Open `m1/a-build-foundation` from that main.
2. Write failing consumer/output/export checks, then implement the bounded
   foundation target. Retain the five PREP CTest cases and all M0 contract tests.
3. Verify shared and static linkage on Linux GCC/Clang and Windows MSVC x64.
   Check shared-library public exports against all 20 accepted functions; do not
   advertise the unsupported subset as implemented functionality.
4. Run C11 and C++20 consumers, buffer canaries and invalid/version/reserved-field
   cases. Run ASan/UBSan for applicable Linux CPU tests. No TSan claim until
   concurrent implementation exists; no physical GPU/security claim.
5. Run a separate clean consumer build linking the foundation library. Pin the
   exact final PR SHA and attach Linux/Windows results. Any failed/missing worker
   leaves the relevant gate incomplete.
6. Hand off one exact-source audit. Report BUILD_FOUNDATION_PASS only when these
   checks pass. Overall G01 runtime implementation, G10 runtime embedding and
   release readiness remain HOLD; successful startup/shutdown is not delivered
   by this foundation-only increment. Obtain the normal per-PR merge decision.

Shared/static testing is build-tree only. SDK install/export packaging, signing,
distributable archives and release tags are later work, with their own scope.

## Explicit dependencies and exclusions

- M1-B: registry, generation/UID nonreuse, lifecycle, trusted security clock,
  source grants/CSPRNG, admission/revocation/expiry, pose provenance and the 87
  actual runtime security scenarios. Required before working runtime handles.
- M1-C: hostile ZIP/GLB/JSON/PNG ingestion, validation budgets, decoder/fuzz corpus.
- M1-D: D3D11 HOST_DEVICE, QUERY_EVENT completion, R-thread COM lifetime,
  state restoration, target/device loss and actual GPU evidence.
- M2 onward: renderer behavior, character visuals, motion and performance targets.

No dependency implementation is authorized by this scope document. Rollback is
to revert the future isolated M1-A implementation merge while keeping M0 design,
PREP tests and public ABI identities. Do not rewrite frozen source history.
