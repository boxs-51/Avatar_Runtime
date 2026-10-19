# Dynamic Cubism L0 preflight evidence — 2026-10-18

**Status:** `PARTIAL`; `HOLD_SDK_OR_LICENSE` and `HOLD_TEXTURE_BUDGET` remain. Branch `m2/live2d-l0-preflight` is stacked on the documentation PR #46. This packet covers only safe local reference/header inventory and a fail-closed CMake gate. It is not MOC3 validation, dynamic rendering, or a G03/G05 promotion.

`scripts/cubism_preflight.py` uses strict JSON keys, bounded file sizes, path containment, duplicate-reference rejection and PNG IHDR/CRC checks before reporting declared decoded RGBA bytes. It reads only MOC3 magic; `moc_core_validation` is always `NOT_RUN` and `png_validation` is `HEADER_ONLY`. It currently accepts the model3 reference kinds present in this sample and rejects other kinds, including declared expressions/motions, as unsupported until their slices are implemented. Its 512 MiB texture ceiling is a private tool safety guard, not an approved runtime cap. `ARS_ENABLE_CUBISM` defaults OFF. An ON configure checks the local SDK header/platform Core path and then fails closed because no Core-backed adapter target exists at L0; this avoids advertising dynamic support from a successful configure.

Local sample command:

```powershell
python scripts/cubism_preflight.py local-assets/ulvm2_0002/ulvm2_0002.model3.json
```

Observed sanitized fields: model3 version 3; MOC3 2,727,808 bytes; five 4096×4096 atlas headers; encoded textures 24,614,672 bytes; declared decoded RGBA 335,544,320 bytes; linked `Physics` and `DisplayInfo`; zero declared expressions and 35 unreferenced `.exp3.json` sidecars. The tool reported `HOLD_SDK_AND_BUDGET`. Current ARS v0.1 pixel ceiling is 134,217,728 bytes. No model, Core binary, frame or derived archive is committed.

Verification on the Windows x64 workspace:

| Command / observation | Result |
| --- | --- |
| `python -m unittest discover -s tests/assets -p test_cubism_preflight.py -v` | PASS: 6/6 synthetic cases, including traversal, duplicates, invalid MOC/PNG headers, duplicate JSON key and declared decode budget. |
| `cmake -S . -B build/cubism-l0-negative -DARS_ENABLE_CUBISM=ON -DARS_ENABLE_PREP_TESTS=OFF` | Expected configure rejection: official SDK header absent. |
| `cmake -S . -B build/cubism-preview -DARS_ENABLE_CUBISM=OFF` | PASS: default build path remains available. |
| `ctest --test-dir build/cubism-preview -C Release -R 'cubism_l0_preflight\|cubism_preview_converter\|m2_private_2d_scene\|m2_warp_2d_pixels' --output-on-failure` | PASS: 4/4 selected tests. |

**Next L0 work requiring external input:** obtain the official Native SDK under accepted terms; pin version and checksum, then implement and run the actual Core validation/version and drawable-feature probe against the private sample. Separately freeze a feasible host-accounted texture budget/residency decision for at least 320 MiB decoded source pixels before L1. Neither absence can be satisfied by the static portrait, by PNG headers, or by CI green.
