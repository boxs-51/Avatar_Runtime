# M1-D private D3D11 spike: exact-source evidence

Source commit: `c7edebd25b179e41b208ebfb5b35b2eec74ccb33` (tree `e30c5231d7e955e8dffa32043ebdedf9a19d10f6`), based on merged C2 `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. The evidence files in this directory are produced from that code commit; this documentation commit does not change the tested GPU code.

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

The shared CTest set passed 33/33 and the static set passed 31/31. D1 readback was RGBA `64,127,191,255`; it rejected null, stale generation, wrong dimensions, another device, wrong thread and an overlapping host SRV. D04 compared every state category touched by the private draw after success, validation rejection and scoped recoverable failure. The original D2 path ran 100 target recreate/event-query retire cycles, each returning BUSY before completion. D07 ran another 100 **close/recreate** cycles on the physical GPU; every close returned BUSY before exact completion, the pin survived BUSY and expired after close, and debug-layer reports before/after showed the same two host-owned device/context identities with no backend object remaining. The WARP test exercised idempotent close, a controlled admission/close race, wrong-thread rejection, S_FALSE and failed GetData, S_OK with false completion data, injected removal/reset, quarantine, teardown acknowledgement and new-device recovery. Injected HRESULTs are not physical device loss.

The preceding PR30 head `ee135e476e59e8f9ddac25114f06bd9400bf3f51` passed [M1-D compile/guard CI](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076141), [M0](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076214), [P0](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076136), [M1-A](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076145), [M1-B3](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076151) and [M1-C2 including both fuzz jobs](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38917076152). The new source/evidence head passed 135/135 local Python contract tests with a writable Windows temp directory; its CI must be checked separately.

| Raw file | SHA-256 |
| --- | --- |
| `M1_D_BUILD_RAW.txt` | `F3420BD52E66E4DC93198075BCB8976341461111D1ABB80EDF92D1355E1D47F2` |
| `M1_D_CTEST_RAW.txt` | `241B7FB96A1E046C1F4D9DE51DB38CD7537647790599DD5BFE8442CBD9CF0715` |
| `M1_D_STATIC_BUILD_RAW.txt` | `A1F0062A2D156FC954E39EC560F037744E0726944F71C8AD21A1C7BF66352FB8` |
| `M1_D_STATIC_CTEST_RAW.txt` | `7DB923520CE862B2DAE0CA4AD5B969E1C3EF96A4B72B47F8E15253F2209BF7B4` |
| `M1_D_D1_RAW.txt` | `E814118CF887052C6D92FCEABA3261659D059B254D226E71F01C310A664B8673` |
| `M1_D_D2_RAW.txt` | `6F8F814AAF32F7A50DBBE44F82930426D508A4F9790EBED5575737FB721AA99F` |
| `M1_D_D4_RAW.txt` | `BD709C0FD95257BD28CC9C98A53760697B053F2E586788E466528F9B02C408D0` |
| `M1_D_D6_RAW.txt` | `D2AC6C048C2E30B381F7489309EFD707D2E3A77CDF4CD5F36E7A9B00034D0199` |
| `M1_D_D7_CLOSE_RAW.txt` | `AB3B8EAA71B945A32F614561FE281938BA8D31BE5D6641404AC71440DB0145EC` |
| `M1_D_D8_INJECTED_RAW.txt` | `4E084ECECEB3C3368D02AB066215A8468B3CD4AAF5913F95E77670A48B962257` |

The evidence is still partial. The user confirmed no dedicated reset-capable runner is available, so no actual device removal/reset was induced on this shared workstation; D08 remains HOLD. The teardown acknowledgement is a host promise, not an independently verified OS barrier. G00 technical-method, full G01/G02 and all 87 B whole-case gates remain HOLD. The private `QueryRetirement` still fails closed if destroyed while pending without completed query or acknowledged lost-device teardown. Public render/drain and the graphics capability bit remain unsupported. This spike does not certify G03/G04/G05, public activation or release.
