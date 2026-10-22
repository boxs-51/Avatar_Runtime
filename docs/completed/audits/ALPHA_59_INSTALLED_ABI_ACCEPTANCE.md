# Alpha #59: ABI20 contract and installed development SDK acceptance

2026-10-20, Asia/Bangkok. Base alpha
`52f3c7366021988d6d035d70c43bf13d22f4e337` after issue58/PR70.
[Issue59](https://github.com/boxs-51/Avatar_Runtime/issues/59),
[current ABI20 contract](../../in-progress/ALPHA_59_PUBLIC_ABI_CONTRACT.md),
[installed-package usage](../../reference/SDK_DEVELOPMENT_PACKAGE.md).

## Result and compatibility boundary

The existing owner-selected20-symbol C surface now has an explicit current
CPU-lifecycle/feature-off behavior table,version/size/error/output semantics,
allocator and H/I/S/R ownership rules,and a separately labelled implementation
handoff for #60/#38/#39. The contract consumes issue58's actual context/worker/
publication inventory and does not mistake its private API for a public symbol.

An opt-in development install exports the real `ARS::RuntimeCore`,the unchanged
canonical header and two development documents. `find_package(AvatarRuntime 0.1
CONFIG REQUIRED COMPONENTS RuntimeCore)` works after physically moving the SDK.
Static/shared variants preserve transitive platform libraries and DLL import
requirements. No source/build-tree include or library paths are embedded in the
installed CMake files. The package option defaults OFF,requires actual RuntimeCore
and excludes sanitizer builds. It installs no foundation/design stub,private C++
header,asset/parser,renderer,worker,model or texture.

The public header,runtime_api.cpp and unsupported.cpp blobs are unchanged.
Model/character/source/update/render/GPU drain remain UNSUPPORTED;caps remain0.
CPU create/capability lookup/close are real. Packaging the bounded development
profile is not public feature activation or a claim that the entire SDK works.

## Actual RED and local direct proof

Baseline `cmake --install build/m2-d-private --config Release --prefix
build/alpha59-red-install` emitted no SDK package. A standalone consumer configured
with that prefix failed because AvatarRuntimeConfig.cmake was absent,recorded in
`build/alpha59-red-consumer.log`. This is the observed installed-contract gap,
not a claim that previous build-tree consumers or source runtime were broken.

The first implemented matrix passed four Windows x64 cases. The final expanded
matrix `python scripts/verify_installed_sdk.py --work-root
build/alpha59-installed-reviewed` also exited0 and reported
`INSTALLED_ABI20_RELOCATED_STATIC_SHARED_PASS cases=4`.
Its receipt at `build/alpha59-installed-reviewed/evidence.json` pins unchanged
public source hashes before/after,per-case installed file lists and binary hashes.
Each command has a numbered log in that directory. Current closure must match
before delivery; source changes require fresh attributable validation.

| Observation | Direct evidence | State |
| --- | --- | --- |
| Static Release and Debug;shared Release and Debug | Four fresh actual library/config installs,physical original-prefix removal and relocated standalone C11/C++20 build/run | PASS |
| All20 symbols and real CPU lifecycle | C11 consumer invokes all20 against implementation,checks zero feature bits/limits,real close and stale/closed outputs | PASS |
| Descriptor/memory ABI | Truly short8-byte record,wrong-major/higher-minor input records,reserved fields,long size/suffix canaries,known output zeroing vs malformed-output no-write | PASS |
| Allocator boundary | Unpaired callback denied,null allocation OUT_OF_BUDGET with zero output;paired host allocation/free match exact bytes/alignment and balance at close | PASS |
| Thread/lifetime | Installed C++20 wrong-thread begin denied;CPU-only separate-thread finalizer succeeds;repeat is stale | PASS |
| Wrong header fallback | Poisoned copied installed header causes compilation failure with exact sentinel | PASS |
| Exact shared ABI | Existing exact20 export checker on both shared configurations | PASS |
| Package selection | Actual configure rejects install-without-core,sanitizer package,incompatible0.2 version,unknown required component,incompatible pointer-width selection and foreign platform | PASS |
| Feature default | Current defaultOFF configure/install emits no SDK;default-off-configure/install logs | PASS |
| Core/security compatibility | Existing18 configured CPU ABI/security regressions,exit0;build/alpha59-core-regression.log | PASS |
| Frozen contracts |178/178 unchanged contracts,exit0;build/alpha59-contracts.log | PASS |

The pointer-width/platform negatives deliberately exercise package selection;
they are not claims of an actual32-bit or foreign-OS library build. C11 is the
consumer source language;static linkage uses the platform C++ link driver as
specified. Release/Debug consumers use matching configurations/CRT. No arbitrary
binary/toolchain mixture is qualified by these checks.

## Independent review and final gates

Separate preliminary source/contract/oracle review found no supported P0-P2;
architecture/project SUPPORTED,optimization NO_RECOMMENDATION. Final independent
current-source/installed-file/runtime reconciliation and committed custody are
recorded at delivery. The source/code task and documentation task are scoped to
#59;no future enabled-model/authority/render runtime behavior is inferred from
this profile or a declaration-only design stub.

Final scoped code/document/local-runtime verdict **PASS**. Independent review
recomputed the current source closure and every installed file hash for all four
variants,verified original prefixes were absent,and independently ran static and
shared Release consumers:each2/2 PASS,exit0,including68 C11 checks and C++ lifetime
checks. Receipts: `build/alpha59-review-static-ctest.log` and
`build/alpha59-review-shared-ctest.log`. No supported remaining P0-P2.

DefaultOFF/current-source ABI compatibility,18 core/security and178 contract
regressions PASS. Catalog156 documents and360 relative links resolve. G1 goal
fidelity,G2 artifacts,G3 installed behavior/negatives,G4 unchanged constraints,
G5 attributable local closure and G6 separate review are satisfied;G7 exact
committed-scope/alpha integration and final CI cross-check receipts are recorded
on the PR/issue. Local execution is primary;other-platform receipts,if obtained,
remain separate observations rather than inherited whole-product qualification.

The [historical ABI map](../../in-progress/V0_1_PUBLIC_ABI_FREEZE_MAP.md) contains
original not-frozen/stub-only text from earlier stages. The current profile
contract and this receipt identify present behavior explicitly without rewriting
historical pins or substituting installed unsupported calls for full G00/G01/
G02/G10 gates. Catalog/index/backlog are current navigation,not release approval.

## Remaining work and limits

Local Windows x64 MSVC evidence is primary. A new PR workflow cross-checks this
same actual install/relocation/consumer matrix on MSVC,GCC and Clang. Until those
runs are observed,non-Windows packaging remains UNVERIFIED;no CI success is
inherited from prior PR70 or inferred from a queue. Linux GPU support is not
advertised. Shipping artifacts/notices,full asset/security/performance/platform
qualification and functional embedded render acceptance remain independent.

Next #60 implements reviewed immutable CPU model/character/source lifetime under
an explicit bounded profile;it must retain feature-off compatibility,isolated
user MOC/PNG/Physics parsing and accepted-state/authority invariants. #38/#39
production context/host integration,physical D06/D08 and public activation remain
HOLD. No main merge,rules/default change,driver reset,SDK dependency adoption or
private-model redistribution occurs in this slice.
