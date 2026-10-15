# M1 implementation preparation

State: PREPARED_FOR_REVIEW / IMPLEMENTATION_NOT_STARTED.
Owner request: prepare M1 implementation. Preparation does not start M1-A,
authorize a merge, or activate the later M1-B/C/D workstreams.

## Verified baseline

Live main: `a307c6775388a507aeacb5fbeb09479633c20f58`, the owner-approved
MERGE_COMMIT for [PR17](https://github.com/boxs-51/Avatar_Runtime/pull/17).
Approved source: `60ee8e29e6682ee956263eb6c1ad1f8f948bec1a`, same tree as main.
Public header blob: `1c61da4a48c9faf2001fc98ad4c084413781e61c`.
The 9 predecessor + 8 G00 blob pins remain the design reference.

PR17's 11/11 source checks passed. Post-merge Profile B run
[#38768615386](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38768615386)
passed. These are M0 design/stub/model results, not M1 results. No PR was open
at the start of this preparation. GitHub branch rules were not changed.

Local tools observed: Python 3.12.10, CMake 4.4.3, Git 2.53.0.windows.2,
Visual Studio 18.8.12023.21 / MSVC 2026. CI's VS2022/GCC/Clang remains the
acceptance matrix; the local toolchain cannot attest those workers.

Governance files retain their original pending-main decision snapshots. Actual
landing is established by PR17 and the main SHA above. Do not reopen accepted
G00 design decisions because historical files still say HOLD; do not interpret
their landing as M1 implementation authority. Updating those snapshots is a
separate documentation scope, not a reason to alter frozen design blobs.

## M1 sequence and usable outcomes

| Order | Deliverable | Entry / exit evidence |
| --- | --- | --- |
| A | C++20 build and ABI foundation | Exact kickoff scope; shared/static libraries, ABI20 exports, C11/C++20 consumers, bounded output checks, Linux/Windows CI |
| B | Runtime IDs, lifecycle and source authority | A integrated; actual nonreuse/pin/admission/clock/revoke/expiry behavior, security tests and sanitizers; renderer-dependent cases explicitly deferred |
| C | Bounded CPU asset ingestion | A integrated, accepted format contract; hostile archive/JSON/PNG/GLB corpus, budgets, reference/graph checks and fuzz evidence before GPU use |
| D | D3D11 HOST_DEVICE interop spike | A integrated, accepted ADR; real Windows GPU/device/state/QUERY_EVENT retirement evidence, reconciled with B lifetime rules |

Implement A first. C and D can be designed after A; neither is activated here.
Combine B/C/D only through declared dependencies and reviewed shared-path custody.
The 19 HDL + 45 SRC + 23 RACE cases are still 87 specified runtime obligations,
not 87 executions. M2 rendering/motion and release packaging remain later work.

## First implementation PR: bounded M1-A

Reference: `docs/M1_A_SCOPE_V0.1.md`. Proposed branch:
`m1/a-build-foundation`, opened only after explicit kickoff. The exact proposed
14-path manifest is `docs/M1_A_IMPLEMENTATION_MANIFEST.json`.

The original scope lists 13 paths. One additional existing test file is necessary:
`tests/contracts/test_m0_consolidation.py`. Its positive case checks mutable
`HEAD` against the exact 19-path M0 delta; any M1 source or preparation document
would fail that historical composition assertion. Its governance mutation test
also intercepts a literal `HEAD:` blob lookup.

Proposed transition, requiring acceptance with the kickoff scope:

1. Keep the M0 consolidation verifier and its negative oracles strict. Run the
   historical composition cases against explicit landed M0 snapshot
   `a307c6775388a507aeacb5fbeb09479633c20f58`, updating mock lookup targets to
   that same snapshot. Do not skip/delete tests or enlarge the M0 19-path list.
2. In the existing provenance checker and a new `test_m1_a_custody.py`, add a
   separate exact M1 branch/base transition: actual current target main,
   accepted source manifest, M0 ancestor and immutable 9+8 blobs at current HEAD.
   An extra runtime/security/graphics path or moved base must fail closed.
3. Before coding, re-read main. If this prep PR lands first, M1's base must be
   that actual new main SHA, not the preparation baseline copied blindly.
   Pin approved preparation documents separately from implementation edits.

This preparation changes no checker or test. A full contract run on a future
docs-prep HEAD is expected to hit the current mutable-HEAD limitation; it must
not be labeled a source regression PASS. The separate transition belongs to A.

Proposed build choices: `ARS_BUILD_FOUNDATION=OFF` by default; explicit ON builds
target `ars_foundation` / alias `ARS::Foundation`. Use `BUILD_SHARED_LIBS=ON/OFF`
in separate clean build directories. PREP libraries/tests keep their identities.
Foundation consumers must link only the foundation target, never the ABI stub.
Use target-scoped C++20, warnings as errors, ELF hidden visibility and explicit
Windows exports; no public C++ types or extra diagnostic symbol.

An external C consumer accepts explicit build-tree library and include paths.
For shared Windows linkage supply its runtime DLL directory to the test process.
No install package, release archive, signing or distribution claim is required.

## Proposed ABI error/output decisions for kickoff

These resolve implementation details left open by the declaration-only design;
they do not change signatures, status numbers, feature bits or the public header.
Accept or revise this table before writing the corresponding behavioral tests.

| Case | Proposed foundation behavior |
| --- | --- |
| ABI version query | Return the existing `(1 << 16) | 0` version |
| Null mandatory descriptor/output; insufficient declared size; incompatible major; known nonzero reserved field | `ARS_E_INVALID_ARGUMENT`; no nested pointer/callback traversal |
| Current v1 descriptor size | Require the known complete v1 layout before reading its fields; reject shorter layouts; this baseline defines no earlier optional-field layout |
| Larger descriptor with compatible major, including a higher minor | Read known fields only; ignore unknown suffix; suffix does not advertise support |
| Valid fixed handle/cap output with any failing call | Zero the documented complete fixed output before returning; never leave the caller's prior token/handle authorized |
| Sized capability/close output with insufficient capacity or incompatible version | Return INVALID_ARGUMENT without writing output fields; caller must provide a readable prefix; never clear an unbounded struct |
| Valid sized capability/close output on unsupported operation | Clear only the known complete layout within declared capacity; preserve suffix canaries; no capability bit set |
| Valid shape after checks | `ARS_E_UNSUPPORTED` for the other 19 APIs; no registry lookup, resource creation, nested asset/channel traversal or callback invocation |

Callers must supply accessible/aligned storage and a readable size/version prefix
as specified by the C ABI. A length field cannot make an inaccessible native
pointer safe; do not probe arbitrary pointer addresses. Descriptor validation
precedes UNSUPPORTED. This foundation has no registry, so it must not pretend
to validate handles, authenticate a bearer, or return a proven stale-handle result.
Runtime create/close success and actual callback behavior remain M1-B.

The PREP stub is unsuitable as implementation: `ars_runtime_query_close` clears
`sizeof(*out)` unconditionally while the design requires caller-capacity bounds;
other stub functions intentionally ignore descriptor validation. Preserve that
nonshipping fixture as historical M0 evidence and implement independent bounded
foundation behavior in the new source paths.

## Ordered work and verification gates

| Step | Work | Required evidence before advancing |
| --- | --- | --- |
| A0 | Record kickoff decision, exact main/header, accepted 14 paths and error table | One M1-A Draft PR; no parent drift or implicit added scope |
| A1 | Write custody and C/C++ consumer/output/export checks first | Actual RED caused by missing foundation behavior, not an unrelated tool failure |
| A2 | Add separate shared/static foundation and ABI version implementation | Clean Linux GCC/Clang and Windows MSVC builds; no PREP stub linked to consumers |
| A3 | Implement bounded error/UNSUPPORTED behavior | Per-case negative results, zero outputs and canaries; callbacks/nested pointers untouched |
| A4 | External C consumer and exact exports | Both linkages; 20 `ars_` functions, no `_p0`, mangled API aliases or optional #21; approved layouts unchanged |
| A5 | CI, regression, sanitizer and final audit packet | All workers completed at one final SHA; explicit HOLDs for unimplemented functionality |

Minimum behavioral case inventory for the evidence report:

| IDs | Coverage |
| --- | --- |
| ABI-01..03 | ABI version, all 20 link references, fixed-width C/C++ layouts |
| BUF-01..05 | Null outputs, zero/short/exact/long capacity, suffix canaries |
| VER-01..04 | Incompatible major, current version, higher minor/suffix, reserved-field rejection |
| OUT-01..03 | Prior nonzero handles/caps zeroed, capability bits zero, bounded close state |
| SIDE-01..02 | Alloc/free/clock callback counters stay zero; nested payloads not consumed |
| BUILD-01..04 | Shared/static, standalone C consumer, export set, target separation |
| PIN-01..05 | Exact base, M0 ancestry, frozen blobs, approved paths, rejected extra paths |

Record each case's configuration, expected/actual status, command/log and final
source SHA; do not report only an aggregate number. Run applicable Linux CPU
tests with ASan/UBSan. TSan and physical GPU tests have no implementation here.
Retain five PREP CTest cases, 83 M0 contract cases (including correctly pinned
historical custody checks), nine governance cases and the new foundation cases.

Acceptance is BUILD_FOUNDATION_PASS. Overall runtime G01, G02, G07/G08, G10,
physical M1-D and SDK release readiness remain HOLD. Successful linkage is not
successful runtime creation or shutdown.

## Preparation handoff and rollback

This branch adds only this document and the proposed JSON manifest. It does not
amend the existing M1-A scope, tests, governance or frozen source. Validate JSON,
manifest uniqueness, header identity and exact documentation-only diff; new
behavioral tests are unnecessary for this preparation-only change. Existing
workflow path filters may schedule no CI for these two new documentation paths;
that is not an M1 CI PASS.

Kickoff needs a scoped owner decision accepting M1-A, the extra historical-test
path and the proposed error table. Future implementation merge remains a
separate exact-PR MERGE_COMMIT decision under local governance. Preparing later
stages does not activate them. No autonomous work between initiated sessions.

Rollback before implementation: close/revert this preparation proposal. After a
future M1-A integration, use a normal reviewed revert of that implementation;
retain M0 history, design pins, PREP evidence and source branches.
