# Pre-M2-D HOLD reassessment — 2026-10-18

**Exact tested source:** `b1ee97465ec60136bd392d06a3500d9c7c4b5a2f` on `m2/pre-d-hold-closure`, stacked on M2-D design PR #35 at `4d8a11f946a3104236ae732cf7769482abed04da`. This audit and raw logs are an evidence-only follow-up. No public ABI/capability, schema, model activation or driver reset changed. Local runner: Windows 10 x64, VS 18 MSVC 19.51.36252.0, SDK 10.0.26100, D3D11 debug layer; Microsoft Basic Render Driver WARP and NVIDIA GeForce GTX 1060 3GB (`10de:1c02`). Build: Release, existing pinned dependency cache, reference-schema checks OFF locally.

The [build](M2_PRE_D_BUILD_RAW.txt), [CTest](M2_PRE_D_CTEST_RAW.txt), [WARP](M2_PRE_D_WARP_RAW.txt), [GTX 1060](M2_PRE_D_HARDWARE_RAW.txt), [WARP live-object](M2_PRE_D_LIVE_WARP_RAW.txt) and [GTX 1060 live-object](M2_PRE_D_LIVE_HARDWARE_RAW.txt) commands all exited 0. `cmake --build build/m2-c-private --config Release --target ars_m2_3d_pixels ars_m2_3d_tests ars_glb_reader_tests ars_glb_validation_tests ars_dual_mode_tests ars_d3d11_live_capture` passed. `ctest --test-dir build/m2-c-private -C Release -R '^(c2_.*|m2_private_3d_scene|m2_warp_3d_pixels)$' --output-on-failure` passed **5/5**. The direct GPU commands were `ars_m2_3d_pixels.exe <corpus>` with no option and with `--hardware`; the debug-capture commands were `ars_d3d11_live_capture.exe --m2c <corpus>` and `--m2c-hardware <corpus>`.

| Raw file | SHA-256 |
| --- | --- |
| `M2_PRE_D_BUILD_RAW.txt` | `1F003534F2993D9DFA8DE607D7C5C2336256F3A3CCA4E567288830DDD6CE9BE8` |
| `M2_PRE_D_CTEST_RAW.txt` | `CBE4530102E83D4335F4D27ED727B0FFCFA3D40785798FD22D304A4E59E1A115` |
| `M2_PRE_D_WARP_RAW.txt` | `6F8248BFAE77C7CDABC8A685A4CDF09D94B83A24BF410FA90DEB50C22528BD6B` |
| `M2_PRE_D_HARDWARE_RAW.txt` | `22AC9DCA8604F0C9FFBC571BA08414F1896A1DFF5B8B8E94348CF4166917DA9B` |
| `M2_PRE_D_LIVE_WARP_RAW.txt` | `E3C3F09FE0B635701CCDECEB383A61E8B511DFBBF772C970FB9C12BF201E77AC` |
| `M2_PRE_D_LIVE_HARDWARE_RAW.txt` | `792DA567AA1013C4A5146FD50924950457C500128198BA2E03367362F2F3CBC8` |

| Subitem reconsidered before M2-D | Direct observation | Disposition |
| --- | --- | --- |
| Independent 3D CPU snapshot pin | A shared prepared snapshot remains live after caller release while the event query owns it, and expires only after exact completion. | Private subitem evidenced; M2-D must use this owned path for host submissions. |
| Host allocator for retained C0 3D snapshot | PMR vectors for draws, vertices, indices, cameras and projection scratch use the provided `assets::Resource`; the host callback count grows, and an injected callback failure preserves the previous prepared snapshot. | Private allocator subitem evidenced. The public host has not supplied its resource or committed pose yet. |
| GPU create failure rollback | Failure before each of 18 D3D creation attempts on the textured draw returns `out_of_budget`; no query pin starts and the prior target pixel stays byte-identical. | Private injected allocation subitem evidenced; physical driver OOM was not induced. |
| Minification/mipmap pixels | All six declared min-filter modes read back on WARP and GTX 1060. On this 2×2 fixture, four mip modes converge to `(99,99,99,255)`; no-mip point stays around `(10,129,10,255)`. | One generated mip level and filter mapping evidenced. Fractional LOD and larger mip-chain oracles remain open; do not call sampler coverage complete. |
| Healthy 3D live objects | Debugger-captured `ReportLiveDeviceObjects` sets match before/after the 3D pass: 10 identity records on WARP and 10 on GTX 1060. | Healthy private resource baseline evidenced; no post-loss or real-host baseline. |

**Gate reassessment:** G04 remains **HOLD** because the production host still does not submit committed 3D pose/camera, the host allocator/owned-snapshot path is not wired into that transaction, fractional LOD/multi-level pixels remain, and integrated resize/close/live-object evidence is absent. G03 and G05 remain HOLD because the 2D host transaction and switch path do not yet exist. D02/D03 remain `HOLD_HOST_INTEGRATION`: their host-wide observations require the M2-D seam. G00 technical-method remains HOLD until a generation-bound teardown barrier and exact-head method review exist. G01/G02 and M1-B G07/G08 remain separate public-runtime/security HOLDs. D06/D08 retain `HOLD_CARRY_TO_M2`: no actual device loss/recovery runner exists; healthy live-object equality and injected failure cannot lift them. D10 remains partial. These findings clear only the named private subitems, so the next code step is M2-D, not gate promotion.
