# M2-A private 2D preparation evidence — 2026-10-17

Tested source: `f89d73d1e9f7ebdf922d12eae26fb33fbae9de65` on `m2/2d-render-foundation`, stacked on M1-D `dffe10c16d5585c0d50fd7745f310543e719c522`. This source commit contains the private scene preparer, CMake switch, native test and CI definition. Evidence files were added afterward without changing tested source.

The local machine ran Windows, Visual Studio 18 x64, `RelWithDebInfo`, with `ARS_BUILD_ASSET_VALIDATOR=ON`, `ARS_BUILD_M2_2D_REFERENCE=ON`, `ARS_ASSET_FETCH_DEPS=OFF` and the existing local dependency cache at `build/c-deps`. The exact source was built using:

```text
cmake --build build/m2-a-private --config RelWithDebInfo --target ars_m2_2d_tests
cmake --build build/m2-a-private --config RelWithDebInfo --target ars_glb_reader_tests ars_glb_validation_tests ars_dual_mode_tests
ctest --test-dir build/m2-a-private -C RelWithDebInfo -R '^(c2_.*|m2_private_2d_scene)$' --output-on-failure
```

The first build exited 0, and CTest exited 0 with 4/4 tests passed: three C2 asset/dual-mode regressions plus the new M2 private 2D preparation test. The 2D test covers draw order, effective opacity, hidden layer exclusion, mesh snapshot ownership, malformed index/triangle/atlas/nonfinite opacity, budget rollback, and an accepted ARSCHAR through the real validator. The three C2 targets built successfully before CTest; their build stdout was observed locally but is not separately retained here. `ARS_ASSET_REFERENCE_CHECKS` was OFF locally, so reference-schema parity was not independently executed in this run. The new CI workflow will build and test the M2 target on Windows and Linux with fetched pinned dependencies.

| Raw file | SHA-256 |
| --- | --- |
| `M2_A_BUILD_RAW.txt` | `3FE8F7DF893E81047C2C28FABD40AA187152159913E73AD4E86A1267E8DA3FA5` |
| `M2_A_CTEST_RAW.txt` | `FC0442356C6AA04E28777A712F5295E6A44C825AFBD0B814D6953D5E2EE17885` |

This is private CPU preparation only. No rendered pixels, GPU lifetime, physical device loss or public ABI integration were tested. G03/G04/G05 and full G00 technical-method/G01/G02 remain HOLD. D06 post-loss lifetime and D08 actual removal/recovery remain `HOLD_CARRY_TO_M2` pending a recoverable GPU runner; no physical PASS is inferred from these tests.
