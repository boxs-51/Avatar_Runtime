# Alpha #54: normalized packed GLB weights and UV

2026-10-20, Asia/Bangkok. **Scoped BUGFIX PASS**, base
`579a04cab90f73d021d88996f0af1d6571d86f94`,
[issue54](https://github.com/boxs-51/Avatar_Runtime/issues/54) in the
[linear backlog](../../in-progress/ALPHA_LINEAR_BACKLOG.md).

## Cause and behavior

The validator retains raw integer accessor storage and normalizes only for
validation. Scene projection previously used those raw WEIGHTS_0/TEXCOORD_0
values. Accepted normalized UBYTE/USHORT weights therefore scaled deformation
by255/65535; UV sampling received integer coordinates.

The projection now converts normalized unsigned components at those two read
sites using the validator's255/65535 divisors. FLOAT and unnormalized values are
unchanged; JOINTS stays a raw index. No weight-sum renormalization, clamp, parser
storage/profile change, ABI/capability, resource-cap or unrelated renderer change.

## Actual RED/GREEN and independent evidence

Twelve public generated fixtures form six packed/float pairs:8/16-bit,
weights-only/UV-only/combined. They cover mixed two-joint transforms, joint1,
zero channels, maxima/intermediate values and a multicolor atlas. CPU controls
compare positions/normals/tangents/UV and indices. GPU controls compare every
channel of the entire8x8 image within1-byte rounding, requiring a nonuniform
reference so an empty image cannot pass. These are synthetic fixtures, not
private-model measurements or general quality-tier/fidelity evidence.

| Requirement | Direct outcome | Result |
| --- | --- | --- |
| Observe both original bugs before production edit | Six CPU and six WARP comparisons FAIL; two CTests exit8 | RED |
| Correct projection and preserve float/joint behavior | CPU geometry/UV pairs agree; joint1 control remains correct | PASS |
| Real render equivalence | All six whole-image pairs PASS on WARP and healthy hardware | PASS |
| Relevant integration regression | GLB reader/validation, dual CPU,3D CPU/WARP and dual-mode WARP:6/6 PASS | PASS |
| Oracle preservation/reproducibility | Every prior corpus record/hash is an identical prefix;12 controls appended; regenerated manifest and reference schema checks agree | PASS |
| Contract regression |178/178, exit0 | PASS |
| Separate review | Reviewer independently ran CPU+WARP2/2 with all6+6 pair checks PASS; inspected hardware/repro/RED; no supported P0-P2 | PASS |

Commands/logs: targeted MSVC Release build `build/alpha54-green-build.log`;
relevant CTest `build/alpha54-green-ctest.log`; before-fix result
`build/alpha54-red-ctest.log`; hardware `ars_m2_3d_pixels <corpus> --hardware`
at `build/alpha54-hardware.log`; full contracts `build/alpha54-contracts.log`;
generator/reference-schema and prefix checks `build/alpha54-corpus-repro.log`,
`build/alpha54-corpus-prefix.log`; independent `build/alpha54-review-ctest.log`.

The initial UBYTE VEC2 fixture lacked4-byte vertex stride and was correctly
rejected. Its log remains `build/alpha54-red-ctest-before-alignment.log`, excluded
from final bug proof. Only fixture padding was corrected under the unchanged
validator; the final RED above admits all controls and observes both bugs.

Architecture/project structure: **SUPPORTED**; optimization **NO_RECOMMENDATION**.
The inherited packed-WEIGHTS P1 and normalized-UV P2 are FIXED_WITH_PROOF for the
validated unsigned profiles. Before publishing, the actual committed candidate
must pass its registered exact10-file alpha PR gate; the resulting PR/issue
records that receipt and integration SHA. Agent review is not GitHub actor-
distinct owner approval, and no remote-CI green is claimed by local execution.

## Reconciliation and completion

Current backlog/catalog reflect this correction; historical acceptance/logs and
all old corpus oracles are preserved. G1 intended behavior, G2 artifacts, G3
observed RED/GREEN/real pixels, G4 constraints, G5 exact base/commands, G6 separate
review and G7 scoped closure PASS, subject to committed-scope/integration delivery
above. Full product remains UNVERIFIED. COLOR, reflection and HRESULT findings
remain in #55..#57; full G04/SDK/performance/platform and physical D06/D08 remain
HOLD. No driver reset, public activation or private-asset redistribution occurs.
