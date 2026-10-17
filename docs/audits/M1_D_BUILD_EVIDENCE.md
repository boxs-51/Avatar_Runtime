# M1-D private D3D11 spike: exact-source evidence

Source commit: `f258812791a666bca9985a3966e062ccdf9f4e49` (tree `6cf71700d87c3bc00b75cfb2fac0820f935d4f20`), based on merged C2 `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. The evidence files in this directory are produced from that code commit; this documentation commit does not change the tested GPU code.

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
```

The shared CTest set passed 33/33 and the static set passed 31/31. D1 readback was RGBA `64,127,191,255`; it rejected null, stale generation, wrong dimensions, another device, wrong thread and an overlapping host SRV. D04 compared every state category touched by the private draw after success, validation rejection and scoped recoverable failure. The original D2 path ran 100 target recreate/event-query retire cycles, each returning BUSY before completion. D07 ran another 100 **close/recreate** cycles on the physical GPU; every close returned BUSY before exact completion, the pin survived BUSY and expired after close, and debug-layer reports before/after showed the same two host-owned device/context identities with no backend object remaining. The WARP test exercised idempotent close, a controlled admission/close race, a 64-submission pending backlog, wrong-thread rejection, S_FALSE and failed GetData, S_OK with false completion data, injected removal/reset, quarantine, teardown acknowledgement and new-device recovery. The candidate reserves tracking capacity before draw so accepted GPU work cannot be left untracked by a later vector allocation. Injected HRESULTs are not physical device loss.

The preceding PR30 head `ee135e476e59e8f9ddac25114f06bd9400bf3f51` passed [M1-D compile/guard CI](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076141), [M0](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076214), [P0](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076136), [M1-A](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076145), [M1-B3](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076151) and [M1-C2 including both fuzz jobs](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076152). The new source/evidence head passed 135/135 local Python contract tests with a writable Windows temp directory; its CI must be checked separately.

| Raw file | SHA-256 |
| --- | --- |
| `M1_D_BUILD_RAW.txt` | `7BCB22D6D19F11FB5C637F11649CE2218850F4EB6CA873162B0ECF12C15D68D7` |
| `M1_D_CTEST_RAW.txt` | `9B838CEC8BE5860BF64FFAEA2E348614A1FE6BA2A66F2921494C07253E23EC42` |
| `M1_D_STATIC_BUILD_RAW.txt` | `ED886BEAE299AA6DC2B8723A5BABB1F5EC4331D4316ECC229261AE3439DCF9F8` |
| `M1_D_STATIC_CTEST_RAW.txt` | `858FDF50177CD8750041A689A5B914D11C95C058C1F1C21BA9C6EFAF2B32E28A` |
| `M1_D_D1_RAW.txt` | `3E407679F45E1B91EE825AE544899EE023BE0D97757758EA6550566584B89313` |
| `M1_D_D2_RAW.txt` | `16FAE710236BCCFC4C9544A12BA76046D8B34948DE1B9DB8A7269CC02A31971B` |
| `M1_D_D4_RAW.txt` | `5A702D3647189B657CCC9F16030A67A5F85D2F0324576DC0D64E0AC1176DFAB8` |
| `M1_D_D6_RAW.txt` | `76ED3E60DE876C41F74330265F605F6F942367FA9E6D3D3B2FCDEAA42C23F823` |
| `M1_D_D7_CLOSE_RAW.txt` | `2DB875E49E23FECC701AA819507B7266CB8F1CCD28D4CE96CD74609EEB5B9561` |
| `M1_D_D8_INJECTED_RAW.txt` | `BC9C6FAC48046D6304E33F2E3FC33004F854F2241E22F7C7DEADF223D09561FF` |

The evidence is still partial. The user confirmed no dedicated reset-capable runner is available, so no actual device removal/reset was induced on this shared workstation; D08 remains HOLD. The teardown acknowledgement is a host promise, not an independently verified OS barrier. G00 technical-method, full G01/G02 and all 87 B whole-case gates remain HOLD. The private `QueryRetirement` still fails closed if destroyed while pending without completed query or acknowledged lost-device teardown. Public render/drain and the graphics capability bit remain unsupported. This spike does not certify G03/G04/G05, public activation or release.
