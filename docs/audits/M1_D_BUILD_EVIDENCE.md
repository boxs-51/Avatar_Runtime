# M1-D private D3D11 spike: exact-source evidence

Source commit: `0bcdca7f8024a1a6765526942591406bdb25a0bd` (tree `b4b38322bda132ff822a3ce3035b7d3f15d30dee`), based on merged C2 `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. The evidence files in this directory are produced from that code commit; this documentation commit does not change the tested GPU code.

Runner: Microsoft Windows 10 Pro 10.0.19045, NVIDIA GeForce GTX 1060 3GB, driver 32.0.15.8157, D3D11 feature level `0xb000` (11.0), VS 2026 MSVC 19.51.36252.0, Windows SDK 10.0.26100.0. The D07 close test created a hardware device with the D3D debug layer; its InfoQueue recorded four explicit live-report warnings and zero errors. WARP supplied deterministic injected-error cases. This is a local physical runner, not a hosted CI GPU result.

Commands run from repository root (all exit 0):

```powershell
cmake -S . -B build/m1-d-regression -G "Visual Studio 18 2026" -A x64 -DARS_BUILD_D3D11_SPIKE=ON -DARS_BUILD_FOUNDATION=ON -DARS_BUILD_RUNTIME_CORE=ON -DBUILD_SHARED_LIBS=ON
cmake --build build/m1-d-regression --config Debug --parallel 1
ctest --test-dir build/m1-d-regression -C Debug --output-on-failure
build/m1-d-regression/Debug/ars_d3d11_host_smoke.exe
build/m1-d-regression/Debug/ars_d3d11_query_retirement.exe
build/m1-d-regression/Debug/ars_d3d11_state_restore.exe
build/m1-d-regression/Debug/ars_d3d11_live_capture.exe
build/m1-d-regression/Debug/ars_d3d11_close_protocol.exe
build/m1-d-regression/Debug/ars_d3d11_live_capture.exe --close
python -m unittest discover -s tests/contracts -p 'test_m1_d_custody.py' -v
cmake -S . -B build/m1-d-static -G "Visual Studio 18 2026" -A x64 -DARS_BUILD_D3D11_SPIKE=ON -DARS_BUILD_FOUNDATION=ON -DARS_BUILD_RUNTIME_CORE=ON -DBUILD_SHARED_LIBS=OFF
cmake --build build/m1-d-static --config Debug --parallel 1
ctest --test-dir build/m1-d-static -C Debug --output-on-failure
cmake --build build/c2-offline-native --config RelWithDebInfo --target ars_glb_reader_tests ars_glb_validation_tests ars_dual_mode_tests --parallel 1
ctest --test-dir build/c2-offline-native -C RelWithDebInfo -R '^c2_' --output-on-failure
```

The shared CTest set passed 33/33 and the static set passed 31/31. D1 readback was RGBA `64,127,191,255`; it rejected null, stale generation, wrong dimensions, another device, wrong thread and an overlapping host SRV. D04 compared every state category touched by the private draw after success, validation rejection and scoped recoverable failure. The original D2 path ran 100 target recreate/event-query retire cycles, each returning BUSY before completion. D07 ran another 100 **close/recreate** cycles on the physical GPU; every close returned BUSY before exact completion, the pin survived BUSY and expired after close, and debug-layer reports before/after showed the same two host-owned device/context identities with no backend object remaining. The same physical harness retained the old RTV across each resize and rejected 198 old-generation/old-size submissions; its 100 close-intent admission cases yielded 50 accepted before close and 50 BUSY after close, followed by R-thread drain and zero debug errors. This narrows D02/D03 private-harness gaps; production host integration remains HOLD. The WARP test exercised idempotent close, a controlled admission/close race, a 64-submission pending backlog, wrong-thread rejection, S_FALSE and failed GetData, S_OK with false completion data, injected removal/reset, quarantine, teardown acknowledgement and new-device recovery. The candidate reserves tracking capacity before draw so accepted GPU work cannot be left untracked by a later vector allocation. Injected HRESULTs are not physical device loss.

The preceding PR30 head `ee135e476e59e8f9ddac25114f06bd9400bf3f51` passed [M1-D compile/guard CI](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076141), [M0](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076214), [P0](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076136), [M1-A](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076145), [M1-B3](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076151) and [M1-C2 including both fuzz jobs](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076152). The new source/evidence head passed 135/135 local Python contract tests with a writable Windows temp directory; its CI must be checked separately.

The available local Windows worker also built the private C2 asset targets from this tree and ran `c2_glb_reader`, `c2_glb_validation` and `c2_dual_mode`: 3/3 passed against the generated 176-fixture corpus. The offline build had `ARS_ASSET_REFERENCE_CHECKS=OFF`; no local reference-schema parity or 3600-second fuzz campaign is claimed by this run. This supports the private CPU parser only, not public G02 model/character activation.

| Raw file | SHA-256 |
| --- | --- |
| `M1_D_BUILD_RAW.txt` | `8A97077F8A2547FB6E4BBD1C94CB03432943BC55F4B2120980862DE2E8AD5AC2` |
| `M1_D_CTEST_RAW.txt` | `36F40574BA553F967B7BCFFF6B75B7C1D2794BE57A474313056C3FEF5A3745EC` |
| `M1_D_STATIC_BUILD_RAW.txt` | `ECCA8135797CDC17C2D7FC2A5E174B9DD78FD0F77913917951CE0F42EE17541A` |
| `M1_D_STATIC_CTEST_RAW.txt` | `FE85FE8A9952FADF0DDEE3BA7DD829E5338316578803FFC66A1239A37CC02BD9` |
| `M1_D_C2_LOCAL_RAW.txt` | `9B152641988CBB7EC4B7FDDBD7B973EEB7984F145F82F22AA93547D68C572E91` |
| `M1_D_D1_RAW.txt` | `3E407679F45E1B91EE825AE544899EE023BE0D97757758EA6550566584B89313` |
| `M1_D_D2_RAW.txt` | `16FAE710236BCCFC4C9544A12BA76046D8B34948DE1B9DB8A7269CC02A31971B` |
| `M1_D_D4_RAW.txt` | `5A702D3647189B657CCC9F16030A67A5F85D2F0324576DC0D64E0AC1176DFAB8` |
| `M1_D_D6_RAW.txt` | `84ADF906143D819B850CFEEF1D20F9871FC7D7942A9EF3C890ABA92DE685A05C` |
| `M1_D_D7_CLOSE_RAW.txt` | `E684D44C2E9A92FE841DDA89BB556F468654F1FE8D1CC95EBAFCF15E31F4EB79` |
| `M1_D_D8_INJECTED_RAW.txt` | `BC9C6FAC48046D6304E33F2E3FC33004F854F2241E22F7C7DEADF223D09561FF` |

The evidence is still partial. The user confirmed no dedicated reset-capable runner is available, so no actual device removal/reset was induced on this shared workstation; D08 remains HOLD. The teardown acknowledgement is a host promise, not an independently verified OS barrier. G00 technical-method, full G01/G02 and all 87 B whole-case gates remain HOLD. The private `QueryRetirement` still fails closed if destroyed while pending without completed query or acknowledged lost-device teardown. Public render/drain and the graphics capability bit remain unsupported. This spike does not certify G03/G04/G05, public activation or release.
