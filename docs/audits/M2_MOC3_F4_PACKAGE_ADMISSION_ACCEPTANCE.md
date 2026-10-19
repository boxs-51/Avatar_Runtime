# Static PNG / Physics3 admission acceptance

2026-10-19, Windows x64 local evidence. **PASS for the authorized static PUB-01 / PUB-03 slice.** Whole F4 remains **REVISE**; release/public activation remain HOLD. No commit, push, merge, SDK capability change or physical GPU reset was performed.

The implementation follows the [frozen design](../M2_MOC3_F4_PACKAGE_ADMISSION_DESIGN.md). The broker transports opaque ZIP bytes; the zero-capability AppContainer resolves the manifest and decodes ZIP, PNG, MOC and linked Physics3 without extraction. Physics3 is checked against the actual native parameter catalog. The trusted host receives raw typed drawable/atlas data, independently validates the entire packet, then allocates an immutable scene and creates GPU resources. It never reparses the public MOC, PNG or physics file.

Physics in this slice is **VALIDATED_NOT_SIMULATED**. The output is a fitted static default pose. Optional motion/expression/pose/user-data/display behavior is explicitly disabled. Persistent public evaluation, physics execution and the final product core/authority publication transaction are not implemented by this diagnostic slice.

## Evidence and outcomes

| Check | Result and direct witness |
| --- | --- |
| Actual package pipeline | PASS, ignored run `build/moc3-package-evidence/f8b4b0a8ad2841cea2191a5b/evidence.json`. Real isolated positive admission, negative PNG/manifest/ZIP/Physics3 paths, worker crash/flood/timeout and cleanup. |
| Independent native packet corpus | PASS: 379 generated rejections, plus valid direct-mask and roundtrip cases. Checks framing, arithmetic/counts, floats, flags, atlas/index references, duplicate/self/nested masks and immutable previous-scene retention. This is bounded mutation evidence, not coverage-guided fuzz. |
| Native Physics3 corpus | PASS: 18 generated rejections, including strict JSON, count/range/bool/catalog errors. Separate actual isolated linked-physics mutations also reject. |
| Actual GPU boundary | PASS: independently malformed worker packets and rejection statuses reach the trusted host with zero device/resource creation. Stale/cancelled request nonces reject; failed replacements retain the earlier image. |
| Numeric/pixel integration | PASS on WARP and physical hardware: generated red/transparent numeric oracle; explicit private resized-atlas fixture equals the trusted-local default-pose readback byte-for-byte on the same backend. This proves pipeline parity, not author equivalence. |
| Query failure | PASS: injected timeout and loss **after submission** produce no candidate frame and exit the dedicated diagnostic process while retaining pending query pins. Timeout is not declared completion or physical device loss. No real device reset occurred. |
| Visible / API proof | PASS, same run's `observer-ui.json` and `observer-api.json`: mapped Tk PhotoImage dimensions/samples, native PPM identity and a separate authenticated API client agree for both backends; unauthenticated request returns 401. Transient descriptor removed. This is a static observer, not an animation API. |
| Existing isolation constraints | PASS, `build/f4-public-preflight-evidence/60b49a81333d4892814c119d/evidence.json`: actual zero-capability/file/child/environment controls, memory/CPU/wall and bounded-pipe probes on the rebuilt worker. Existing limited loopback observation remains limited; no universal network-denial claim. |
| Generated broker tests | PASS, 5 tests: custody is not validation; launcher/worker/wait/overflow/cleanup failures cannot return a ready packet. |
| Affected regression selection | PASS all 27 selected checks across two recorded invocations. Initial selection passed 26; authority races were NOT_RUN because its executable was absent. Built `ars_source_authority_race_tests`; that check then passed, exit 0. Logs: `build/public-package-final-ctest.log`, `build/public-package-authority-ctest.log`. The entire baseline was not run. |
| Provenance / independent review | PASS: parent captures source/dependency/binary pins **before child creation**, compares after exit; child separately captures before/after test execution. All agree and still match current files. [Public-safe closure](M2_MOC3_F4_PACKAGE_ADMISSION_SOURCE.json), [independent review](M2_MOC3_F4_PACKAGE_ADMISSION_REVIEW.md), [goal assessment](M2_MOC3_F4_PACKAGE_ADMISSION_GOAL_ASSESSMENT.json). |

The original private full-resolution atlas set exceeds this profile's **128 MiB combined decoded RGBA** limit and correctly returns `PACKAGE_BUDGET_REJECTED`. Successful private pixel tests use a separately labelled, explicitly generated resized-atlas fixture, preserving its MOC and linked physics. Import does not resize user assets or fall back to trusted parsing. The 512 MiB worker, 2 CPU seconds, 5 wall seconds and 192 MiB packet caps remain fixed. No model files, pixels, identifiers, geometry/atlas counts, asset hashes or model-derived measurements are exported into these documents/manifests.

Earlier diagnostic runs remain available: `6561bb939eed410ba5dd85d4` failed because the loss hook fired before query submission; `d1f0090821034dfe80e65c14` failed the typed atlas-budget status assertion. Both were corrected without weakening limits or expected results. `16ed17a8b4c847a988ae7845` supplies preliminary functional evidence only: the broker source changed after module import, and its observer client encountered shutdown transport reset. It is excluded from final source/execution closure. The final run freezes sources before process creation and stops API polling at terminal phase before waiting for cleanup.

## Remaining boundaries

Persistent isolated public animation/physics, final product publication/authority, shipping SDK/ABI, original over-budget models, coverage-guided fuzz, integrated sanitizer memory acceptance, representative performance tiers, broader network/non-Windows qualification and redistributable fixtures remain outstanding. D06/D08 physical loss/recovery stays HOLD until a reset-capable disposable runner exists. Static admission PASS does not close those gates.

Reproduce with `python scripts/moc3_verify_admission_observer.py` after building the optional targets as described in the design. The local machine is the primary evidence source; CI remains corroborating.
