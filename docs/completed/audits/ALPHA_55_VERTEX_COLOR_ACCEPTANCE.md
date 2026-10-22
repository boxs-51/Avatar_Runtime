# Alpha #55: linear vertex color through offscreen rendering

2026-10-20, Asia/Bangkok. **Scoped BUGFIX PASS.** Base `2c335e8120e3946e5ee931143d68359e0f8a6b4b`.
[Issue55](https://github.com/boxs-51/Avatar_Runtime/issues/55),
[ordered backlog](../../in-progress/ALPHA_LINEAR_BACKLOG.md).

## Contract and correction

Validated COLOR_0 was retained by admission but omitted from CPU scene projection
and the GPU vertex base factor. Valid assets therefore rendered silently without
vertex color. The private snapshot now retains linear RGBA, default white, with
RGB alpha1. FLOAT stays linear; normalized UBYTE/USHORT use255/65535 divisors.
GPU vertex base is material base factor multiplied by vertex color, interpolated
by the existing shader and combined with texture and alpha behavior. No shader
lighting, GPU vertex layout, parser profile or public ABI/capability change.

Private color preflight rejects nonfinite/out-of-range values before any GPU
allocation, preserving the target. CPU pose accounting uses sizeof(Vertex3D),
so added color storage stays inside the unchanged pose cap. GPU resource caps,
worker limits, main and private model data remain unchanged.

## Evidence

42 deterministic public fixtures were appended: six FLOAT/UBYTE/USHORT RGB/RGBA
profiles, each with three colored/material-baked pairs for OPAQUE/MASK/BLEND and
one varying-color triangle. Baked controls have no COLOR_0 and independently
multiply numeric material factors in the generator, exposing a renderer that
ignores color. Full8x8 frames compare every channel within1-byte rounding.

| Requirement | Direct evidence | Result |
| --- | --- | --- |
| Observe original defect | All18 colored/material-baked WARP comparisons failed before production edits; CTest exit8 | RED |
| Decode supported color forms | Six CPU gradient profiles, raw numeric RGB/RGBA expectations, missing-color white, unchanged pose | PASS |
| Apply material factor and alpha | All18 whole-frame pairs and six varying controls pass WARP and healthy hardware; visible reference required | PASS |
| Boundary and preflight | RGBA MASK exclusion; negative, above1, infinity and NaN rejected with zero GPU creates and unchanged target | PASS |
| Relevant integration | GLB reader/validator, dual CPU,3D CPU/WARP and dual WARP:6/6 | PASS |
| Preserve oracles | All old corpus entries/hashes identical prefix;42 appended; complete manifest regeneration/reference schema | PASS |
| Contract regression |178/178, exit0, same suite with required Windows permission | PASS |
| Separate review | Independent CPU/WARP2/2 PASS, source/test review no supported P0-P2 | PASS |

Commands and ignored local logs: targeted MSVC Release build at
`build/alpha55-green-build.log`; CTest regex
`^(c2_glb_reader|c2_glb_validation|c2_dual_mode|m2_private_3d_scene|m2_warp_3d_pixels|m2_warp_dual_mode)$`
at `build/alpha55-green-ctest.log`; before-fix
`build/alpha55-red-ctest.log`; `ars_m2_3d_pixels <corpus> --hardware` at
`build/alpha55-hardware.log`; generator with committed manifest and
`--reference-schema` at `build/alpha55-corpus-repro.log`; original prefix at
`build/alpha55-corpus-prefix.log`; full contracts at
`build/alpha55-contracts.log`. Independent review log: `build/alpha55-review-ctest.log`. The first full contract
run hit Windows sandbox temp-cleanup WinError5 (178 run,1 error), preserved at
`build/alpha55-contracts-sandbox-temp-error.log`; the same suite is rerun with
the required Windows permission and passed178/178, without changing assertions.
Initial test build warnings were corrected before
the successful run; they are not behavioral proof.

## Reconciliation and gate

The focused candidate must pass its exact12-file alpha custody gate after
commit; the issue/PR records the actual source and integration SHA. Local proof
is primary, with no remote-CI pass inferred. Historical corpus/report oracles
are unchanged. Backlog/catalog/index track this scoped correction.

G1 intended behavior, G2 artifacts, G3 actual offscreen/negative behavior,
G4 constraints, G5 attributable local proof and G6 separate source/test/doc
review PASS. G7 scoped closure PASS subject to actual committed-scope and
integration receipts recorded in the issue/PR. Architecture/project structure:
SUPPORTED; optimization NO_RECOMMENDATION. Agent independence does not claim
GitHub actor-distinct owner approval. This document does not close
product qualification. D3D failure classification #56 and reflected transforms
#57 remain open. Persistent session/public ABI/SDK, full G04 and physical D06/D08
remain HOLD. No reset, activation or private-asset redistribution occurred.
