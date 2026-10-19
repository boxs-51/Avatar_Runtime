# M1-A build foundation evidence

Scope: BUILD_FOUNDATION, not a working runtime or SDK release. Same-account
technical-method verification; INDEPENDENCE_NOT_AVAILABLE. Implementation
kickoff is the owner's explicit YES to PR18's scope/plan in the active chat,
including its 14-path manifest and ABI error/output table. Merge is separate.

Base main: `a307c6775388a507aeacb5fbeb09479633c20f58`.
Approved preparation: PR18 `5119645829d63b96d7cb9d9898703607e82d9ab1` (not merged).
Implementation: [PR19](https://github.com/boxs-51/Avatar_Runtime/pull/19),
`m1/a-build-foundation`, directly on main, without an implicit prep-PR merge.
The final source SHA and completed CI runs are attested in the PR description;
embedding a document's own final commit hash here would require changing it.

## Implementation and source identity

Exactly 14 paths from the accepted manifest are changed. The header remains
blob `1c61da4a48c9faf2001fc98ad4c084413781e61c`; all 9 predecessor + 8 reviewed
G00 blobs remain unchanged. The current M1 gate requires exact main ancestry,
the exact 14-path delta and frozen baseline manifest values. M0 composition
tests now target explicit landed M0, preserving every negative oracle and the
original 19-path gate; M1 current source has its own gate and seven negative/
positive custody tests. No tests are skipped to obtain GREEN.

`ARS_BUILD_FOUNDATION=ON` opts into a separate C++20 `ars_foundation` target;
default builds retain PREP only. `BUILD_SHARED_LIBS=ON/OFF` selects independent
builds. Public headers/types/status values are unchanged. Shared builds define
the correct producer/consumer macros; static consumers do not import a DLL.
No foundation consumer links either historical PREP stub.

ABI version is implemented. Other API shapes validate readable size/version
prefixes, full known v1 layout and reserved fields before returning UNSUPPORTED.
Fixed handle/cap outputs are zeroed on failure. Invalid sized outputs are not
written; valid sized outputs clear only their known layout, preserving suffix
canaries. No runtime registry, callback invocation, nested payload traversal,
source grants, parsing or graphics work is implemented. Native callers remain
responsible for accessible/aligned storage; this is not a hostile-pointer sandbox.

## RED-first evidence

Initial test commit: `529cc59982bf9319aa8a7c00744bc6dffee5cee7`.
The new C behavior test was compiled with local MSVC against the original M0
design DLL before foundation implementation: 185 checks, **105 failures**, exit 1.
Compilation succeeded; failures include INVALID_ARGUMENT precedence and short
output no-write assertions, not a missing compiler. Raw local output is retained
at ignored `build/integration-evidence/m1-behavior-red.txt`.

Actual remote initial RED runs:
- [M1-A PR #38770854887](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38770854887)
- [M1-A push #38770843435](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38770843435)
- [M0 PREP #38770854758](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38770854758)

At that test-only head the new branch/base transition and historical-test pinning
were absent. Remote RED demonstrates the old provenance boundary rejecting new
source, not native foundation behavior; local compiled RED supplies that separate
behavioral witness. [P0/R5 #38770854759](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38770854759)
already passed at the RED head. Final CI must re-attest everything at one SHA.

## Native behavioral inventory and local verification

`foundation_contract.c` emits an expected/actual result per check, with IDs:

| Cases | Observed behavior under test |
| --- | --- |
| ABI-01/02 | Accepted version and all 20 public functions linked/called |
| ABI-03 | C11/C++20 fixed-width x64 layouts and all 20 C++ calling signatures |
| BUF-01/02 | Null mandatory arguments and zero/short/readable-prefix-only storage |
| BUF-03/04/05 | Exact/long layouts, compatible higher minor, suffix canaries, invalid-output no-write |
| VER-01/03/04 | Wrong major rejected; unknown minor/suffix allowed; every known reserved field rejected |
| OUT-01/02/03 | Nonzero prior handles/caps cleared; no capability features; bounded close state |
| SIDE-01/02 | Host alloc/free/clock counters remain zero; nested arrays/assets not traversed |
| BUILD-02/03 | External C11 build-tree consumer and exact 20 unmangled shared exports |
| PIN tests | Exact base/ancestry/path set, missing export checker, header mutation and unauthorized source/test additions |

Local tools: Windows x64, MSVC 19.51.36252.0 (VS2026), Windows SDK 10.0.26100.0,
CMake 4.4.3. Shared configure/build **PASS**, CTest **8/8 PASS**, including the
five PREP tests, bounded C test (initially 185 checks, zero failures), C++ test and actual
DLL export inspection. Static configure/build **PASS**, CTest **7/7 PASS**.
External shared and static C11 consumers separately configured, built and each
passed **1/1**. Complete local source regressions: **90/90** contract tests and
**9/9** governance tests PASS, along with the original provenance script. An
additional actual-library negative probe ran the export checker on the legacy
P0 DLL and correctly rejected its wrong symbol set. Logs are retained in ignored
`build/integration-evidence/m1-exports-negative.txt` and
`m1-behavior-green.txt`. Local results never substitute for CI's VS2022 workers.

Final review added actual eight-byte heap allocations for both sized output
APIs, checking INVALID_ARGUMENT and zero writes rather than only using a short
declared capacity inside a full-size object. The final C behavioral suite has
189 checks and also asserts all nine descriptor/output layouts in C11. Prefix
negotiation reads size/version through bounded byte copies before accessing
typed tail fields, so a full T object is not assumed for prefix-only storage. Final CI
must re-attest these new prefix-only cases under ASan/UBSan and both linkages.

Reproduction, from the source root (use VS2022 in CI, VS2026 on this workstation):

```text
cmake -S . -B build/foundation -DCMAKE_BUILD_TYPE=Debug -DARS_BUILD_FOUNDATION=ON -DBUILD_SHARED_LIBS=ON
cmake --build build/foundation --config Debug --parallel
ctest --test-dir build/foundation -C Debug --verbose
cmake -S examples/plain_c -B build/consumer -DARS_FOUNDATION_INFO=<absolute-build-dir>/foundation-Debug.cmake
cmake --build build/consumer --config Debug --parallel
ctest --test-dir build/consumer -C Debug --verbose
```

Repeat in separate directories with `BUILD_SHARED_LIBS=OFF`. Linux uses GCC and
Clang independently, plus a GCC shared run with
`ARS_ENABLE_FOUNDATION_SANITIZERS=ON` for ASan/UBSan on foundation/consumer CPU
targets. PREP targets are regression checks, not sanitizer-certified by that flag.

## Final acceptance and remaining holds

The M1 workflow requires seven workers: Linux GCC/Clang shared/static, Linux GCC
shared ASan/UBSan, Windows MSVC shared/static. Each runs the full contract and
governance suites, stage/provenance checks, PREP + foundation CTest and external
consumer. Per-case CTest logs are uploaded as workflow artifacts; raw job logs
also retain expected/actual results and the source HEAD. Existing impacted PREP
and P0/R5 workflows must pass at the same final head.

Only completed, successful final-SHA results support BUILD_FOUNDATION_PASS.
Failed/missing jobs, sanitizer failures or export drift must be repaired first.
G01 working runtime, G02 real asset ingestion, 87 production security cases,
M1-D physical GPU, G10 working embedding and release readiness remain HOLD.
No TSan or independent actor-review claim. No install/release packaging added.

Rollback: a normal reviewed revert of the eventual M1-A merge, preserving M0
history, public header/design pins and original evidence. Keep PR19 unmerged
until a separate exact-head owner MERGE decision.
