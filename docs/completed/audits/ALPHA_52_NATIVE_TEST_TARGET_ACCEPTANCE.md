# Alpha #52: configured native test custody

2026-10-19, Asia/Bangkok. **Scoped BUGFIX PASS**, base `a17d4f9` on alpha.
Contract: [issue52](https://github.com/boxs-51/Avatar_Runtime/issues/52), A01 in
the [linear backlog](../../in-progress/ALPHA_LINEAR_BACKLOG.md). Required
outcomes are current CMake/configuration target selection, failure for missing
required targets, two-directory/configuration negative proof and native regression.

## Cause and correction

Registered Python tests previously selected fixed developer Release directories.
An alternate build/configuration could exercise another binary or skip green.
CMake now supplies generator-expanded TARGET_FILE paths for native physics,
packet publication and step publication. The shared resolver rejects explicit
empty, relative or missing targets; explicit settings never fall back. The
publication client launches the same selected path its tests check. Standalone
diagnostics keep optional fallback only when no explicit setting is supplied.
The provenance capture includes the new resolver and regression source.

No production C++, ABI, capabilities, worker limits, shaders or physics method
changed. Native executables used here were built at integrated31ce916; CMake
regeneration changed test registration, not their implementation. Full prior
66-test integration and its separate contract RED are recorded in the
[alpha audit](ALPHA_INTEGRATION_AND_ARCHITECTURE_AUDIT_20261019.md).

## Actual RED/GREEN and independent evidence

| Requirement | Direct evidence | Result |
| --- | --- | --- |
| Required target must fail closed | Before fix, the3 missing-target requests ignored configuration and succeeded; original generated suite reports9 failing subcases | RED observed |
| Correct configured target/config | Two tiny real CMake builds, Release exit73 and Debug exit74; each of the3 scripts actually launches its selected sentinel, writes its distinct marker and fails | GREEN |
| Real native regression | Integrated targeted4/4 CTest: publication7/7, step5/5, native Python/physics reference, target-custody2/2; no skips | PASS |
| Second actual build | Isolated native-reference CTest1/1; generated CTest JSON for both product build directories has their own TARGET_FILE values | PASS |
| Independent review | Separate `/root/f2_independent_review` inspected exact diff and independently ran targeted4/4 once elevated, exit0, no skips | PASS |

Commands/logs: `python -B tests/moc3/test_native_test_targets.py` before fix:
`build/alpha52-red.log`, exit1,2 tests/9 failures. `cmake -S . -B build/m2-d-private`
then targeted `ctest --test-dir build/m2-d-private -C Release -R '^moc3_(native_test_target_custody|native_physics_reference|packet_publication|step_publication)$' --output-on-failure`:
`build/alpha52-configure.log`, `build/alpha52-green-ctest.log`, exit0.
Second configure and reference execution: `build/alpha52-isolated-configure.log`,
`build/alpha52-isolated-ctest.log`, exit0. Target registration proof:
`build/alpha52-current-tests.json`, `build/alpha52-isolated-tests.json`, checked
by `build/check_alpha52_binding.py`. Independent receipt:
`build/alpha52-review-ctest.log`. Sentinel Debug proof is target dispatch evidence,
not qualification of the full production runtime in Debug or another platform.

Architecture assessment **SUPPORTED**; project structure **SUPPORTED**;
optimization **NO_RECOMMENDATION**. No supported new P0/P1/P2. A-01 is
FIXED_WITH_PROOF. No GitHub actor-distinct approval or whole security gate is
claimed by the separate agent review.

## Reconciliation and completion

PDRM: current backlog maps this correction; prior acceptance/source manifests
remain historical and untouched. Historical conventional-directory proofs are
not retroactively invalidated. Source/test/docs match the bounded issue contract;
catalog and relative links are refreshed. No migration/shared-DB impact applies.
G1 goal fidelity, G2 artifacts, G3 actual negative/native outcomes, G4 protected
constraints, G5 revision/command evidence, G6 independent review and G7 closure
coherence all PASS for this issue's code/document scope. Integration and issue
closure require the resulting focused PR to land in alpha. Full product remains
UNVERIFIED. #53 contract custody and #54..#57 rendering findings remain OPEN.
