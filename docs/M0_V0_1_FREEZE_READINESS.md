# M0 v0.1 Freeze-Readiness Ledger RC1 — Candidate, NOT Approved

**ID:** ARS-M0-G00-FREEZE-RC1  
**Security parent:** Draft PR #2 @ 16abfb058a90f90f4ed89f6b6ced2000bff0b30f
**Current decision:** \`HOLD_G00_CONTRACT_FREEZE\`. Do not merge this prep PR or the parent PRs.
**Read with:** docs/V0_1_CANONICAL_CONTRACT_RC0.md and docs/M1_SOURCE_BOOTSTRAP_PLAN.md.

**GPU design candidate:** `docs/ADR_0001_D3D11_G00_DESIGN_RC1.md` defines HOST_DEVICE-only v0.1 proposal and separates G00 decision from later M1-D hardware proof. No reviewer approval or actual GPU evidence yet.

## RC1 audit remediation register (owner proposals, no independent approval)

| Audit finding | Remediation | Current disposition |
| --- | --- | --- |
| G00-PR3-01 P1 | G00 ABI declaration-only design and signature smoke; G01 actual library, G10 real C consumer | DESIGN_PROPOSED; full ABI still HOLD |
| G00-PR3-02 P1 | G00 ADR-0001 HOST_DEVICE design + experiment manifest; M1-D physical GPU validation later | DESIGN_PROPOSED; ADR owner/reviewer HOLD |
| G00-PR3-03 P1 | Select 2D PROFILE B, VRM1 disabled, semantic validator phases and exact BAD_ASSET/UNSUPPORTED/OUT_OF_BUDGET taxonomy; add executable schema/semantic design checks | DESIGN_PROPOSED; format reviewer HOLD |
| G00-PR3-04 P2 | Pin parent SHA and nine schema/ABI/security blobs; CI checks schema examples, negative mutations and cross-document precedence | CI IMPLEMENTATION PENDING until terminal run |

**Current canonical candidate:** `docs/V0_1_CANONICAL_CONTRACT_RC1.md`; previous RC0 is historical. Parent pin is `docs/M0_CONTRACT_PARENT_PINS.json`. F-01..F-10 remain HOLD until independent approvals; F-11 remains PREPARED. M0 G00 is a DESIGN gate, not a demand for native D3D11 hardware or runtime parser validation.

## Exact decisions still required

| ID | Required artifact / question | Available evidence | Disposition | Decision owner |
| --- | --- | --- | --- | --- |
| F-01 | Independent exact-HEAD R5/R6 security design review | R5 owner handoff, 87 gate definitions; no R5 independent verdict at preparation | HOLD | independent architecture/security reviewer |
| F-02 | One final C ABI incl all public lifecycle, asset, character, render, capabilities functions | \`contracts/ars_p0_contract_draft.h\`: 14 symbols, x64 C/C++ stub link CI GREEN | HOLD (incomplete public API) | ABI owner + auditor |
| F-03 | Registry/epoch/lease/capability state and lock-order ADR | R5 contract proposed; deterministic RACE-L..W contract-model subset GREEN | HOLD (not ratified) | runtime/security owners |
| F-04 | ADR-0001 D3D11 HOST_DEVICE/SDK_DEVICE, immediate context, fence/state restore/device-loss | spec/roadmap proposal only | HOLD (blocks GPU implementation) | graphics owner + reviewer |
| F-05 | ADR-0003 2D rig mesh/relative transform/mask binding | manifest/ars2d schema and examples, mismatched scope against spec | HOLD | asset/2D owners |
| F-06 | ADR-0005 monotonic security time vs animation clock, expiry/fault semantics | R4/R5 proposed, model subset GREEN | HOLD until independent ratification | security/clock owner |
| F-07 | Semantic JSON+cross-file validation and negative fixtures | two schemas and examples; no required compiled validator | HOLD | asset/format owner |
| F-08 | ABI version/struct_size/calling convention/alloc ownership and error behavior | partial x64 header/layout smoke | HOLD until complete consumer/source contract | ABI owner |
| F-09 | Schema/manifest capabilities: \`vrm1\` deferred, 2D mesh counts, IDs, defaults and layer targets | identified design gaps; no post-schema checker | HOLD | format owner |
| F-10 | G00 decision with exact merged design candidate HEAD, reviewer, raw evidence and sign-off | no independent pass; PR #1/#2 both DRAFT | HOLD | release/governance owner |
| F-11 | M1 source scopes and non-production build bootstrap | scope proposal in M1 plan; CMake prep-only scaffold | PREPARED, not G01 PASS | implementation owner |

**Decision details:** F-02/F-08 require `docs/V0_1_PUBLIC_ABI_FREEZE_MAP.md`; F-05/F-07/F-09 require `docs/V0_1_FORMAT_VALIDATION_FREEZE_MAP.md`. Neither document represents an accepted/frozen API or parser.

## What is verified (limited scope)

- PR #2 head \`16abfb058a90f90f4ed89f6b6ced2000bff0b30f\`, Draft and not merged at preparation.
- GitHub Actions run \`38512133801\` at that head is GREEN for Linux C11/C++20 and Windows MSVC x64 **stub-linked ABI and deterministic R5 contract-model only**. Raw logs include RACE-T/V/W PASS. No production runtime.
- The source/security negative matrix consists of **19 HDL + 45 SRC + 23 RACE = 87 specified cases**. Treat all real runtime assertions as HOLD until an implementation with reproducible CI exists.
- Proposed normative precedence is R5 > R4 > R3 > R2 > first P0 design > baseline sketch, *only where revisions explicitly supersede older paragraphs*. A consolidated candidate entry point now exists; the historical documents still need reviewer disposition.

## Pre-freeze PR ordering and signoff

1. Obtain independent R5 DESIGN verdict pinned to exact PR #2 HEAD. Fix new findings on PR #2 if any; update this ledger and re-pin downstream branch.
2. Lock ADR-0001/0003 and cross-file asset semantics with explicit signed decisions, not assumptions.
3. Complete the v0.1 public C header and normative size/version/ownership/error semantics, then compile/link a plain-C consumer with **real** implementation or clearly identify an ABI declaration-only design gate.
4. Cross-validate JSON Schema and semantic fixture matrix; require negatives for duplicates, bool-vs-number, default range, positive atlas rect, vertex/UV/delta counts, triangle bounds, VRM capability deferral and path canonicalization.
5. Reconcile contradictions between legacy pointer sketch, by-value handles, ID-driven source capability, v0.1 source fields, host-owned D3D11 and mode/renderer ownership.
6. Obtain fresh independent M0/G00 review on one candidate SHA; separately track production G01–G11 after code exists.
7. Only an explicit downstream authority decision can promote \`CONTRACT_RC\` to \`FROZEN\`; reviewer design approval does not automatically authorize merge. Do not merge in this task.

## Definition of actionable states

- \`PREPARED\`: docs/source manifest and test scaffolding exist; work may be reviewed.
- \`DESIGN_REVIEW_READY\`: internal contradictions resolved, every high-risk ADR has a proposed answer and repeatable design evidence.
- \`DESIGN_PASS\`: independent exact-HEAD reviewer accepts the scoped design.
- \`G00_PASS\`: all baseline format/API/device/clock/security design obligations have accepted evidence and authority.
- \`M1_OPEN\`: G00 PASS + source ownership/CLAIM + negative gate plan + build/test contract are recorded.
- \`G01_PASS\`: source builds reproducibly on required OS and passes actual CTest; bootstrap CMake alone is **not** this gate.
- \`PRODUCTION_READY\`: never inferred from \`DESIGN_PASS\` or an ABI stub; requires actual security/graphics/release gates.

## Immediate deliverables from this preparation

- Single candidate entry point \`docs/V0_1_CANONICAL_CONTRACT_RC0.md\`.
- G00 HOLD checklist (this file).
- \`docs/M1_SOURCE_BOOTSTRAP_PLAN.md\` with exactly-scoped M1-A/B/C/D and concurrency/security dependencies.
- A **prep-only, nonshipping** CMake + CTest harness for original C/C++ ABI smoke and R5 contract-model, with mandatory opt-in wording. No production \`ars/ars.h\` or engine code is defined by this preparation.

**Owner status:** READY FOR FREEZE REVIEW, **NOT FROZEN**; external independent design review still required, and no merge is authorized.
