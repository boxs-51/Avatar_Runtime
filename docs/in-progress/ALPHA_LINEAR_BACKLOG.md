# Alpha: ordered implementation contract and issue backlog

2026-10-19, Asia/Bangkok. **Current execution order.** This document coordinates
the existing stage contracts; it does not replace frozen ABI/security oracles.
Integration source: `31ce916a72a08af27b3d87daa793b5e43cb50394`, baseline
`ec30b6c6b305f8d495864e3a33818d07106a1abd` plus divergent PR44. Main stays
`8723ef174a7db98b05fd031aa39dbd910e5ba2db`. See the
[source/document audit](../completed/audits/ALPHA_INTEGRATION_AND_ARCHITECTURE_AUDIT_20261019.md)
and [product HOLD ledger](M2_MOC3_F4_RELEASE_LEDGER.md).

## C0 user contract and boundaries

The owner requested Codex Engineering & Design Workflow: merge development PRs
into a new `alpha`, close those PRs, audit architecture and repository documents,
normalize documentation, update open issues into a linear executable list, then
open focused branches to fix/develop in that order. This authorizes integration,
issue maintenance, documentation and scoped implementation/commit/push. It does
not request a main merge, default-branch/rules change, shipping activation or
driver reset. Private model/derived assets remain local and excluded from Git.

| Requirement | Acceptance and proof |
| --- | --- |
| REQ-A1 integration | Preserve main and every development PR head, including divergent44, through explicit merge commits into alpha; verify ancestry and record the merge receipt |
| REQ-A2 disposition | Close integrated development and superseded preparation PRs honestly; distinguish local merge into alpha from GitHub merged-into-main status; keep all source branches/history |
| REQ-A3 audit/docs | Independent source-grounded architecture/project review; classify current/planned/historical records, normalize navigation/catalog while preserving frozen evidence and negative oracles |
| REQ-A4 issues | Update existing issues6,38..43 and add nonduplicate ordered work with dependencies, scopes, acceptance and external HOLD entry criteria |
| REQ-A5 implementation | Focused branch/PR per runnable slice, observed bug baseline, relevant local verification and independent review; no whole-product completion from one slice |

## Linear local work

Finish each row before starting its successor; if a design needs a material
policy decision, prepare the concrete decision before dependent code. An issue
contains its source finding and exit evidence. The external physical gate is at
the end so it does not block runnable local bug fixes.

| Order | Issue | Scope / acceptance | State at audit |
| --- | --- | --- | --- |
| A01 | [#52](https://github.com/boxs-51/Avatar_Runtime/issues/52) | Bind required Python/CTest checks to the configured binary/configuration; missing targets fail; two-directory sentinel negatives | INTEGRATED via [PR63](https://github.com/boxs-51/Avatar_Runtime/pull/63), alpha4b05828; [proof](../completed/audits/ALPHA_52_NATIVE_TEST_TARGET_ACCEPTANCE.md) |
| A02 | [#53](https://github.com/boxs-51/Avatar_Runtime/issues/53) | Pin original M2-E source plus ancestry; preserve historical allowlists; separate alpha checks; reconcile canonical paths and narrowly route alpha PRs | INTEGRATED via [PR64](https://github.com/boxs-51/Avatar_Runtime/pull/64), alpha579a04c; [proof](../completed/audits/ALPHA_53_CUSTODY_ACCEPTANCE.md) |
| A03 | [#54](https://github.com/boxs-51/Avatar_Runtime/issues/54) | P1 normalized packed WEIGHTS and normalized UV; integer/float geometry and pixel equivalents | INTEGRATED via [PR65](https://github.com/boxs-51/Avatar_Runtime/pull/65), alpha2c335e8; [CPU/WARP/hardware and independent proof](../completed/audits/ALPHA_54_PACKED_ACCESSORS_ACCEPTANCE.md); issue records exact committed/integrated SHA |
| A04 | [#55](https://github.com/boxs-51/Avatar_Runtime/issues/55) | Apply supported COLOR_0 or typed rejection before GPU | INTEGRATED via [PR66](https://github.com/boxs-51/Avatar_Runtime/pull/66), alpha76dc628; local CPU/WARP/hardware,178 contracts and independent [proof](../completed/audits/ALPHA_55_VERTEX_COLOR_ACCEPTANCE.md) |
| A05 | [#56](https://github.com/boxs-51/Avatar_Runtime/issues/56) | Typed allocation/removal HRESULTs and old-target quarantine | INTEGRATED via [PR67](https://github.com/boxs-51/Avatar_Runtime/pull/67), alphada805b4; typed errors/rollback/replacement,7 related tests,178 contracts and independent [proof](../completed/audits/ALPHA_56_D3D_FAILURE_ACCEPTANCE.md) |
| A06 | [#57](https://github.com/boxs-51/Avatar_Runtime/issues/57) | Reflected inherited transforms: winding/normal classification and numeric mirror controls | INTEGRATED via [PR68](https://github.com/boxs-51/Avatar_Runtime/pull/68), alpha37ba135; static CPU/WARP/hardware,178 contracts and independent [proof](../completed/audits/ALPHA_57_REFLECTED_TRANSFORM_ACCEPTANCE.md) |
| A07 | [#58](https://github.com/boxs-51/Avatar_Runtime/issues/58) | Inventory production context/ownership from #38 as a design input, freeze session resource/input policy, then persistent isolated lifecycle/replacement | R0 design plus opt-in R1-R5 implementation and direct local [acceptance](../completed/audits/ALPHA_58_SESSION_ACCEPTANCE.md); R1-R5 direct local and independent source/oracle review PASS,69 native tests and178 contracts; exact committed custody/alpha integration recorded in issue/PR before closure |
| A08 | [#59](https://github.com/boxs-51/Avatar_Runtime/issues/59) | Reviewed public model/input/render ABI, ownership/statuses and installed consumers | [ABI20 profile/ownership contract](ALPHA_59_PUBLIC_ABI_CONTRACT.md) and opt-in relocated development SDK [acceptance](../completed/audits/ALPHA_59_INSTALLED_ABI_ACCEPTANCE.md); local static/shared Release/Debug and independent installed-consumer review PASS;18 core/security and178 contracts PASS; exact committed custody/integration recorded on issue/PR;capability remains OFF |
| A09 | [#60](https://github.com/boxs-51/Avatar_Runtime/issues/60) | Approved public CPU asset/authority lifecycle, resource/close/replay negatives and installed C consumers | IN PROGRESS: [A60-0/A60-1a isolated verdict probe](ALPHA_60_ISOLATED_ADMISSION_DESIGN.md) and [direct local evidence](../completed/audits/ALPHA_60_ISOLATED_ADMISSION_ACCEPTANCE.md); [typed 2D model transfer](ALPHA_60_TYPED_2D_TRANSFER_DESIGN.md) now has [local acceptance](../completed/audits/ALPHA_60_TYPED_2D_TRANSFER_ACCEPTANCE.md); full F1-F5 now have [local Windows acceptance](../completed/audits/ALPHA_60_PUBLIC_CPU_ACCEPTANCE.md), including complete GLB/dual, runtime ownership, atomic authority publication and installed four-config consumers; final committed integration is recorded on issue60 |
| A10 | [#38](https://github.com/boxs-51/Avatar_Runtime/issues/38) | Production context-user inventory, admission barrier and generation-bound teardown proof | HOLD production scope; private PR44 is present |
| A11 | [#39](https://github.com/boxs-51/Avatar_Runtime/issues/39) | Production committed pose/camera and native2D/GLB3D host publication acceptance | HOLD integration; separate G03/G04/G05 |
| A12 | [#41](https://github.com/boxs-51/Avatar_Runtime/issues/41) | Full public asset/build G01/G02 matrices after CPU+host work | HOLD gate assessment |
| A13 | [#42](https://github.com/boxs-51/Avatar_Runtime/issues/42) | All87 production security scenarios with exact per-case proof | HOLD whole-case qualification |
| A14 | [#61](https://github.com/boxs-51/Avatar_Runtime/issues/61) | Integrated fuzz/sanitizer/performance/platform/fixture qualification | HOLD prerequisites; no substitute from short batches |
| A15 | [#40](https://github.com/boxs-51/Avatar_Runtime/issues/40) | Disposable Windows GPU runner and actual loss/recovery packet | EXTERNAL HOLD: runner absent; no reset here |
| A16 | [#62](https://github.com/boxs-51/Avatar_Runtime/issues/62) | Reconciled gate/release evidence, artifacts/notices and explicit activation decision | HOLD release |

Issue6 is the operating tracker; issue43 records supersession of the old main
merge plan by alpha consolidation. Issue dependencies express implementation
order, not permission to close an upstream gate with an incomplete sub-slice.

## Working procedure

Session/ABI design at A07/A08 must first consume the production context-user and
ownership inventory from #38; production barrier implementation remains A10.
This avoids freezing lifetime rules before identifying the real owners.

Refresh alpha/source/reviews, freeze bounded issue acceptance, then branch
`fix/alpha-<issue>-<topic>` or `dev/alpha-<issue>-<topic>`. Record RED for bug fixes,
implement without weakening assertions, verify actual configured binaries and
appropriate native/API/visible evidence, obtain independent review, reconcile
current docs and submit a focused PR to alpha. Merge only within the owner's
current authorization and applicable rules; close an issue after its evidence
and integration, with the exact SHA. Do not rotate authority, raise caps or
discard old state to make a test pass. Avoid unrelated refactors.

Local Windows results are primary; CI is a cross-check. A held physical/platform
or fixture prerequisite stays explicit. Full SDK/product goal remains UNVERIFIED.
