# M2-B private offscreen pixel evidence — 2026-10-17

Exact tested source: `6b0a59ed991f2f86e3c20040da64b7314b775558` on `m2/2d-offscreen`, stacked on M2-A `10f542f7acb10527bace0dca6d7c608b20018896`. The source commit contains the D3D11 pass, state capture expansion, pixel contract, test and CI workflow. This evidence commit adds only the report and raw logs.

Local system: Windows 10.0.19045, Visual Studio 18, Windows SDK 10.0.26100, x64 `RelWithDebInfo`; assets were built with `ARS_ASSET_FETCH_DEPS=OFF` using pinned local cache `build/c-deps`, `ARS_ASSET_REFERENCE_CHECKS=OFF`. Enabled options: `ARS_BUILD_D3D11_SPIKE=ON`, `ARS_BUILD_ASSET_VALIDATOR=ON`, `ARS_BUILD_M2_2D_REFERENCE=ON`, `ARS_BUILD_M2_D3D11_2D=ON`. The GPU path used NVIDIA GeForce GTX 1060 3GB, vendor `10de`, device `1c02`, D3D feature level `b000`, with D3D11 debug layer and **zero debug messages**.

Commands after the source commit:

```text
cmake --build build/m2-b-private --config RelWithDebInfo --target ars_m2_2d_pixels ars_m2_2d_tests ars_d3d11_state_restore ars_d3d11_host_smoke ars_d3d11_close_protocol
ctest --test-dir build/m2-b-private -C RelWithDebInfo -R '^(m2_.*2d_.*|d1_physical_.*|d7_warp_close_protocol)$' --output-on-failure
build/m2-b-private/bin/RelWithDebInfo/ars_m2_2d_pixels.exe --hardware
```

Build exited 0. CTest exited 0 with **5/5 PASS**: two M2 2D cases, two D1 physical GPU regressions and the D7 WARP close protocol. The M2 pixel case reads back UV endpoints including out-of-range clamp without neighbor bleed, layer order, premultiplied opacity, hidden and empty output, resize and pre-submit invalid-input rollback; it also checks extended IA/VS/PS state restoration, exact event-query completion and a zero-warning/error debug queue when the debug layer is available. The named hardware run exited 0 with the same pixel assertions and debug count 0. The pixel checker permits a two-byte UNORM rounding tolerance; it does not use a visual-only oracle.

| Raw file | SHA-256 |
| --- | --- |
| `M2_B_BUILD_RAW.txt` | `939981A7AB6F8369C16F4A37A5E45337EA2ED15A8208F41BD03F236CE0D1691F` |
| `M2_B_CTEST_RAW.txt` | `EB20963F1637FE7A45C53A58DA291907621D8D63703DFDB6E78E6D8D0E2C1B72` |
| `M2_B_HARDWARE_RAW.txt` | `C8F49BF510C83612B257DAF3C658401DF9A407DB5937F3BE3C944499298E1F5A` |

This is **private M2-B pixel evidence**, not G03 acceptance. The path has no public runtime/host admission integration, does not prove repeated dynamic hide/show through the public API, and does not close the full G00 technical-method, G01 or G02 gates. D06 post-loss lifetime and D08 actual device loss/recovery remain `HOLD_CARRY_TO_M2`: no driver reset was attempted on this shared machine. G03/G04/G05 remain HOLD pending their direct integrated checks; WARP and normal physical rendering do not substitute for actual loss/recovery evidence.
