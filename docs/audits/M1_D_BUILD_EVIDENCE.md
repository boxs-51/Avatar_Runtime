# M1-D private D3D11 spike: exact-source evidence

Source commit: `f51375ae74e32f2bdabba29aa3549967468e3c24` (tree `2a4d85672edc584051a744f0d0d91cf077f55275`), based on merged C2 `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. The evidence files in this directory are produced from that code commit; this documentation commit does not change the tested GPU code.

Runner: Microsoft Windows 10 Pro 10.0.19045, NVIDIA GeForce GTX 1060 3GB, driver 32.0.15.8157, D3D11 feature level `0xb000` (11.0), VS 2026 MSVC 19.51.36252.0, Windows SDK 10.0.26100.0. The D2 test created a device with the D3D debug layer. Its InfoQueue recorded 14 live-report warnings and zero errors; the warnings are expected from two explicit live-object reports while backend state is alive. This is a local physical runner, not a hosted CI GPU result.

Commands run from repository root (all exit 0):

```powershell
cmake -S . -B build/m1-d-regression -G "Visual Studio 18 2026" -A x64 -DARS_BUILD_D3D11_SPIKE=ON -DARS_BUILD_FOUNDATION=ON -DARS_BUILD_RUNTIME_CORE=ON -DBUILD_SHARED_LIBS=ON
cmake --build build/m1-d-regression --config Debug --parallel 1
ctest --test-dir build/m1-d-regression -C Debug --output-on-failure
build/m1-d-regression/Debug/ars_d3d11_host_smoke.exe
build/m1-d-regression/Debug/ars_d3d11_query_retirement.exe
build/m1-d-regression/Debug/ars_d3d11_state_restore.exe
build/m1-d-regression/Debug/ars_d3d11_live_capture.exe
python -m unittest tests.contracts.test_m1_d_custody -v
cmake -S . -B build/m1-d-static -G "Visual Studio 18 2026" -A x64 -DARS_BUILD_D3D11_SPIKE=ON -DARS_BUILD_FOUNDATION=ON -DARS_BUILD_RUNTIME_CORE=ON -DBUILD_SHARED_LIBS=OFF
cmake --build build/m1-d-static --config Debug --parallel 1
ctest --test-dir build/m1-d-static -C Debug --output-on-failure
```

The shared CTest set passed 31/31 and the static set passed 29/29. D1 readback was RGBA `64,127,191,255`; it rejected null, stale generation, wrong dimensions, another device, wrong thread and an overlapping host SRV. D04 captured and compared every state category touched by the private draw: IA topology, VS/PS shader identity (zero class instances), rasterizer, two viewports, all eight RTV slots, DSV, blend state/factor/mask and depth state/stencil reference. Identity and values matched after a real draw, invalid-generation rejection and scoped recoverable failure. D2 ran 100 target resize/recreate and event-query retire cycles. Every cycle returned BUSY at least once (`busy=100` total); the snapshot witness and target reference stayed held until the matching query completed. Wrong generation and wrong thread polling were rejected. Debug-layer live-object reports captured seven identical object identities before and after 100 cycles, with no growth. This compares the loop lifetime, not post-close teardown. No runtime close was exercised.

The first PR30 CI run at evidence commit `244e6a3423ca721884367352fff4d75d97eb7bdd` passed both Windows compile and Linux guard jobs ([run 38910470237](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38910470237)). An additive C2 custody-test repair then pinned its historical candidate to `0eb1560e0cdfa5a7f44b77efc8dbf559dbc014ef` instead of later `HEAD`, without changing GPU source. PR-head CI for this updated source must be checked separately.

| Raw file | SHA-256 |
| --- | --- |
| `M1_D_BUILD_RAW.txt` | `BD123C9FF0289D419CB35FA3C6E933ED04CAE65E0D5CCE586B699EA186F54243` |
| `M1_D_CTEST_RAW.txt` | `D81088ACC16A3160EFBCEF883BD67129C8A39A43BEA8C14D40631E9EC742386F` |
| `M1_D_STATIC_BUILD_RAW.txt` | `0BB53383A709570B44ABACE914608D187088C058DC8FEEEC6AC11AF57A83DD3C` |
| `M1_D_STATIC_CTEST_RAW.txt` | `77BB6C240B68902C60B8913CE1C77E081124616C8DF8CDBBD07C0E11A53E49E1` |
| `M1_D_D1_RAW.txt` | `E814118CF887052C6D92FCEABA3261659D059B254D226E71F01C310A664B8673` |
| `M1_D_D2_RAW.txt` | `6F8F814AAF32F7A50DBBE44F82930426D508A4F9790EBED5575737FB721AA99F` |
| `M1_D_D4_RAW.txt` | `BD709C0FD95257BD28CC9C98A53760697B053F2E586788E466528F9B02C408D0` |
| `M1_D_D6_RAW.txt` | `FE95B00EF1769EBC0B6BC5EAF324B341972E93BEDB3F90C587B358D25C659327` |

The evidence is deliberately partial. G00 technical-method review remains HOLD. There is no device removal/reset injection, no host close with in-flight GPU work, no post-teardown live-object count, no physical Linux GPU execution (not in scope), and no independently reviewed source/physical result. The private `QueryRetirement` fails closed if destroyed while pending; its quarantined teardown path is not implemented, so device-loss/close acceptance remains HOLD. The public render/drain symbols and capability bit remain unchanged and unsupported. Full G02 and 87 B whole-case gates retain their previous HOLD states. This spike does not certify G03/G04/G05, public activation or release.
