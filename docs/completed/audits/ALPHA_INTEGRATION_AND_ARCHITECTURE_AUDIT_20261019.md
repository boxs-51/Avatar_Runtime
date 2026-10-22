# Alpha integration and architecture audit

2026-10-19, Asia/Bangkok. Owner explicitly requested integration to **new alpha**,
PR closure, architecture/document audit, normalized docs and ordered issues.
[Current contract and issue order](../../in-progress/ALPHA_LINEAR_BACKLOG.md).

## Integration custody and disposition

Main was `8723ef174a7db98b05fd031aa39dbd910e5ba2db` and is preserved. The primary
agent created alpha there, then merged with no-ff commits in order:
**37 ->44 ->45 ->46 ->47 ->48 ->49 ->50 ->51**. Aggregate source is
`31ce916a72a08af27b3d87daa793b5e43cb50394`. Every source head is an ancestor.
PR44 is not an ancestor of PR51; merging only the latest stack would have lost
its receipt implementation. Relative to final PUB04-B baseline ec30b6c, the
aggregate adds exactly PR44's10-file delta,259 insertions/24 deletions. No merge
conflicts occurred; no main/rules/default-branch changes or force pushes.

These are local Git merge commits published to alpha. All15 original open PRs
were verified closed after publication (9 integrated,6 superseded). Receipt:
`build/alpha_pr_closures_final.json`; remote open-PR search returned none at this
checkpoint, before subsequent focused fix PRs. Original PRs are closed
as integrated into alpha, not represented as GitHub merged into their old bases
or main. Historical preparation PRs18/20/22/25/26/29 are superseded; their original
branches/commits and closed PR conversations remain accessible. Their obsolete
root planning files are not reintroduced into current documentation. Existing
implementation and current backlog supersede that planning, not its provenance.

Repository ruleset BV25099333 applies deletion/non-fast-forward protection to
main/master. It does not authorize bypasses; none was changed or used. Commit
status endpoints for37/44/51 returned no statuses, which is not CI success.
Workflow-run inspection separately found historical37/44 runs successful, while
PR51@ec30b6c has failed M1-A39266139539, M0-prep39266139654 and M1-B39266139571;
M1-C39266139586 was still in progress at inspection. These are explicit remote
failures/pending, not overridden by local results. Diagnose them under #53 and
do not claim fresh exact-alpha CI success. The local integrated run is primary.

## Independent architecture and structure inspection

Separate reviewer `/root/f2_independent_review` inspected frozen ec30b6c and the
bounded PR44 delta at31ce916 using Git objects while integration changed the
workspace. The reviewer made no source edits and ran no runtime workloads in
this audit. Its source/math findings are distinguished from observed executions.

| Surface | Actual boundary / evidence | Assessment and limit |
| --- | --- | --- |
| Public ABI / build | include/ars/ars.h; src/abi; optional CMake targets | ABI20 frozen; model/character/render operations remain deliberately unsupported; private binaries are not shipping SDK |
| Core / security / motion | src/core, src/security, src/motion; final CoreConsumer path | Ownership, handles/source authority and immutable CPU publication are coherent; not all87 production scenarios executed |
| Native assets | src/assets and src/validation | Owned CPU data and resource checks precede GPU; isolated schema worker is resource containment, not universal sandbox proof |
| MOC3 | src/moc3/package_admission, checkpoint/step packets, packet_publication | User decoding/physics isolated; hostile typed output fully checked before GPU; exact query then final core/checkpoint/image acceptance |
| Graphics / host | query_retirement, close_coordinator, teardown_receipt, d3d11_dual_mode_host | Clear owner/query/lifetime boundary; PR44 move-only single-use receipt checks authority/device/generation/owner/consumption; only its private host ownership is proven |
| 3D projection | scene3d and d3d11_scene3d | Accepted-feature defects remain in normalization/color/orientation and typed D3D failure paths; complete G04 not qualified |
| Tests / docs | CMake registration, Python helper discovery, historical pin checks, catalog | Workstation paths can select stale binaries; historical HEAD check and moved document paths need repair; structure CONCERN |

Architecture assessment: **SUPPORTED within inspected boundaries**. Project
structure: **CONCERN**. Optimization proposals: **NO_RECOMMENDATION**; no broad
rewrite is justified. The MARM coexistence concern is private independent-MOC3
alongside native2D/GLB3D, not a shared-database migration: clear ownership and
opt-in build boundaries are retained, no dual-write/cutover is introduced.
Concurrency/sanitizer/fuzz/platform completeness remains UNVERIFIED.

## Findings and traceability

| Finding | Status / evidence | Issue |
| --- | --- | --- |
| Test binaries hardcoded to developer Release directories | NEW P2; test_native_physics_reference.py:13 and moc3_publication_probe.py:8; alternate CMake builds may skip/use stale targets | #52 |
| Historical M2-E positive check on evolving HEAD | NEW P2 on alpha; exact10-path original scope cannot accept the aggregate; observed contract failure | #53 |
| G00 manifest resolver uses pre-relocation HEAD paths | Observed3 contract errors at HEAD:docs/P0_OWNER_REMEDIATION_V0.5.md; preserve frozen blobs and repair path/revision custody | #53 |
| Packed WEIGHTS / normalized UV | Inherited PR37 P1/P2 review findings; source reads raw normalized integer values | #54 |
| COLOR_0 silently ignored | Inherited PR37 P2 | #55 |
| Generic failure for D3D OOM/removal | Inherited PR37 P2 | #56 |
| Reflected GLB winding/normal classification | NEW P2 source/math proof; runtime RED not yet run | #57 |

PR44 fixes the old M2-D HEAD custody by pinning ef28 with ancestry; do not reopen
that resolved issue. Existing PR37 review threads stay unresolved until actual
fix evidence exists; closing the PR for consolidation is not resolving findings.
Source subject needs revision and includes inherited P1; release subject remains
BLOCKED/UNVERIFIED. Delivering this audit is not an approval of those defects.

## Actual local execution and remaining verification

Windows x64 MSVC Release build exited0: `build/alpha-integration-build.log`.
Integrated CTest **66/66 PASS**, exit0: `build/alpha-integration-ctest.log`.
This includes PR44 private teardown controls and existing CPU/WARP/healthy GPU
components. Some Python checks use the conventional directory that was actually
built, so #52 remains mandatory for alternate configurations. No final visible
checkpoint rerun is claimed on alpha; previous PUB04-B source-pinned evidence
remains historical at ec30b6c.

Full Python contract discovery at integrated31ce916 executed160 tests: **FAIL**,
1 failure(M2-E path drift) and3 errors(G00 old document path). Raw log:
`build/alpha-contract-baseline.log`. This is direct RED for #53, not an integration
PASS. The independent source review does not invalidate prior PUB04-B proofs
that explicitly pinned the conventional build directories; #52 covers unqualified
alternate-directory/configuration behavior.

## Documentation reconciliation and goal assessment

README/ledger/index are CURRENT_STATE; the new linear backlog is current execution
coordination; effective frozen security/ABI documents stay NORMATIVE within their
original scope; old stage reports and raw captures are HISTORICAL_REFERENCE.
Older plans remain proposed/historical and cannot grant implementation or merge
authority. PR44's maintained plan moves into in-progress with original bytes;
the original relocation map remains a historical artifact. Catalog/navigation
gain the new records, without editing historical source manifests/logs or model
assets. No test oracle is weakened to make source fit a document.

PDRM01..04 inventories scope/baselines above;05..07 maps findings to issues/current
docs;08..11 preserves optional ABI/build/ownership and migration boundaries;
12..16 requires exact integration/test/review/catalog evidence. No shared DB
MARM migration applies. Architecture audit task is complete with findings;
code subject requires revision; product goal remains UNVERIFIED. Subsequent
implementation follows the issue chain with independent evidence per slice.
Physical D06/D08, persistent PUB02/full PUB04, shipping SDK, platform/sanitizer/
performance and rights-cleared fixtures remain HOLD. No asset redistribution,
activation, driver reset or gate promotion occurs.
