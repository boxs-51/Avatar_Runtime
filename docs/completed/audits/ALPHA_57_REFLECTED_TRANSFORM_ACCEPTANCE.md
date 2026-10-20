# Alpha #57: reflected static transforms and safe skin rejection

2026-10-20, Asia/Bangkok. **Scoped BUGFIX PASS.** Base `da805b4a361fca11dda766d3943c4b8369c34691`.
[Issue57](https://github.com/boxs-51/Avatar_Runtime/issues/57),
[ordered backlog](../../in-progress/ALPHA_LINEAR_BACKLOG.md).

## Scope and correction

Static scene projection formerly transformed positions/normals but kept the
original triangle indices and tangent handedness. An inherited odd reflection
therefore rendered as a back face under the camera-wide front-face convention:
single-sided geometry disappeared; double-sided geometry used the back normal.

The CPU snapshot now reverses each static triangle once when the global linear
transform determinant is negative, and flips tangent handedness. Existing
inverse-transpose normals stay unchanged. Positive determinants, including two
inherited reflections, keep their index order. Camera/shader/GPU layout, source
accessor values, public ABI and resource caps remain unchanged. Singularity and
nonfinite determinant fail before publishing the prepared snapshot.

Skinning can have different blended transforms across vertices. A draw-wide
flip based on one bone or mesh node would be unsafe. The private projection
computes each vertex's effective weighted linear skin transform and returns
UNSUPPORTED for reflected or singular blends, retaining the previous snapshot.
This is an explicit supported-profile boundary, not general reflected-skin
support. Testing includes a negative blend of three individually positive
rotation matrices, so checking only bone determinants cannot substitute.

## Direct evidence

Nineteen generated public controls are appended without modifying any old
corpus record/hash. Eight authored-transform/baked pairs cover direct negative
TRS, an inherited reflection, an equivalent decomposable matrix, and two
inherited reflections; each has single/double-sided materials. Baked references
independently transform positions, preserve normals and provide expected index
order/tangent handedness. Three skin cases cover joint, inherited and mixed
rotation reflection with transactional rejection.

| Requirement | Observed result | Status |
| --- | --- | --- |
| True baseline reproduction | Six CPU orientation failures, two even-reflection controls PASS; three skin-rejection failures;12 pixel failures/four even controls PASS | RED |
| Static CPU projection | Eight pairs match positions, normals, tangent and indices | PASS |
| Safe skin boundary | Three valid admitted GLBs return UNSUPPORTED with old source/position retained | PASS |
| Real offscreen image |16 full8x8 comparisons PASS on WARP and healthy hardware across both camera parity conventions, with visible numeric PBR reference{0,75,0,255} and1-byte pair tolerance | PASS |
| Relevant integration | GLB reader/validator, dual CPU,3D CPU/WARP and dual WARP:6/6 PASS | PASS |
| Corpus custody | Old records/hashes identical ordered prefix;19 appended; committed manifest regeneration and reference schema checks PASS | PASS |
| Full contracts |178/178, exit0 | PASS |
| Independent review | Separate CPU/WARP2/2 PASS with all new controls; no supported P0-P2 | PASS |

Final logs: `build/alpha57-red-ctest.log`, `build/alpha57-green-build.log`,
`build/alpha57-green-ctest.log`, `build/alpha57-hardware.log`,
`build/alpha57-corpus-prefix.log`, `build/alpha57-corpus-repro.log`,
`build/alpha57-contracts.log`. Commands use MSVC Release targets
`ars_m2_3d_tests`, `ars_m2_3d_pixels`, `ars_m2_dual_pixels`; the six related CTests;
`ars_m2_3d_pixels <corpus> --hardware`; fixture generator with committed manifest
and `--reference-schema`; full unittest discovery under `tests/contracts`.

The first matrix fixture included shear, which the unchanged admission profile
correctly rejected. Its log `build/alpha57-red-before-matrix-profile.log` is
preserved and excluded from final bug proof. The matrix control was corrected
to a decomposable reflected affine transform before the final RED above; no
validator or assertion was weakened.

## Reconciliation and delivery

Independent receipt: `build/alpha57-review-ctest.log`. Architecture/project
structure SUPPORTED; optimization NO_RECOMMENDATION. G1 intended behavior, G2
artifacts, G3 CPU/numeric pixel RED/GREEN plus rejection/rollback, G4 constraints,
G5 attributable local evidence and G6 separate source/runtime review PASS. G7
scoped closure PASS subject to actual committed-scope/integration receipts below.
Agent independence does not claim GitHub actor-distinct owner approval. Full
product remains UNVERIFIED. The actual
committed candidate must pass its exact10-file alpha custody gate, with source
and integration SHA recorded in issue/PR. Current backlog/catalog/index track
this scoped correction; frozen historical records/oracles remain unchanged.
Local execution is primary; no remote-CI success is inferred.

Persistent sessions/public ABI, full SDK/product/performance qualification and
physical D06/D08 remain unresolved/HOLD. Healthy offscreen hardware results do
not establish destructive physical-loss recovery. No main merge, activation,
private-model publication or driver reset occurs.
