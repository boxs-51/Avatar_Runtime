# M1-D private D3D11 spike: exact-source evidence

Source commit: `e1511c33726f1f247e5825753e77e0f27948b5e7` (tree `d1841fe3266f82927d59b58f70e03e7838defa62`), based on merged C2 `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. The evidence files in this directory are produced from that code commit; this documentation commit does not change the tested code.

Runner: Microsoft Windows 10 Pro 10.0.19045, NVIDIA GeForce GTX 1060 3GB, driver 32.0.15.8157, D3D11 feature level `0xb000` (11.0), VS 2026 MSVC 19.51.36252.0, Windows SDK 10.0.26100.0. The D2 test successfully created a device with the D3D debug layer; its InfoQueue recorded zero messages and errors in that run. This is a local physical runner, not a hosted CI GPU result.

Commands run from repository root (all exit 0):

```powershell
cmake -S . -B build/m1-d-regression -G "Visual Studio 18 2026" -A x64 -DARS_BUILD_D3D11_SPIKE=ON -DARS_BUILD_FOUNDATION=ON -DARS_BUILD_RUNTIME_CORE=ON -DBUILD_SHARED_LIBS=ON
cmake --build build/m1-d-regression --config Debug --parallel 1
ctest --test-dir build/m1-d-regression -C Debug --output-on-failure
build/m1-d-regression/Debug/ars_d3d11_host_smoke.exe
build/m1-d-regression/Debug/ars_d3d11_query_retirement.exe
python -m unittest tests.contracts.test_m1_d_custody -v
cmake -S . -B build/m1-d-static -G "Visual Studio 18 2026" -A x64 -DARS_BUILD_D3D11_SPIKE=ON -DARS_BUILD_FOUNDATION=ON -DARS_BUILD_RUNTIME_CORE=ON -DBUILD_SHARED_LIBS=OFF
cmake --build build/m1-d-static --config Debug --parallel 1
ctest --test-dir build/m1-d-static -C Debug --output-on-failure
```

The shared CTest set passed 29/29 and the static set passed 27/27. D1 readback was RGBA `64,127,191,255`; it rejected null, stale generation, wrong dimensions, another device, wrong thread and an overlapping host SRV. IA topology, viewport, rasterizer, RTV, blend, depth/stencil and VS/PS binding samples were restored after a real draw. D2 ran 100 target resize/recreate and event-query retire cycles. Every cycle returned BUSY at least once (`busy=100` total); the snapshot witness and target reference stayed held until the matching query completed. Wrong generation and wrong thread polling were rejected. No runtime close was exercised.

The first PR30 CI run at evidence commit `244e6a3423ca721884367352fff4d75d97eb7bdd` passed both Windows compile and Linux guard jobs ([run 38910470237](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38910470237)). An additive C2 custody-test repair then pinned its historical candidate to `0eb1560e0cdfa5a7f44b77efc8dbf559dbc014ef` instead of later `HEAD`, without changing GPU source. The full Python contract suite passed 132/132 with a writable Windows temp directory. PR-head CI after that repair must be checked separately.

| Raw file | SHA-256 |
| --- | --- |
| `M1_D_BUILD_RAW.txt` | `1B196F880AB13C7909D82D807E4828E1F557F75F009C42871DCB619FA655B8C8` |
| `M1_D_CTEST_RAW.txt` | `7C2746F123195C44E8DE11E948D2861CED841254D29C146453F7B4DE81499AAC` |
| `M1_D_STATIC_CTEST_RAW.txt` | `7A8219498632904276CDBB80BAD4D077E4BA84C0C18532427E99B4116F1F6498` |
| `M1_D_D1_RAW.txt` | `CE54C940443A86F32062DBA1C572D72444DCFEFC0F2EA03D9E48272C7278C3E8` |
| `M1_D_D2_RAW.txt` | `AF197A4D2B966AD52B47A4BDF12813FEB470E0C5FFA0AFCF46C5367626DCDC00` |

The evidence is deliberately partial. G00 technical-method review remains HOLD. There is no device removal/reset injection, no host close with in-flight GPU work, no post-teardown live-object count, no physical Linux GPU execution (not in scope), and no independently reviewed source/physical result. The private `QueryRetirement` fails closed if destroyed while pending; its quarantined teardown path is not implemented, so device-loss/close acceptance remains HOLD. The public render/drain symbols and capability bit remain unchanged and unsupported. Full G02 and 87 B whole-case gates retain their previous HOLD states. This spike does not certify G03/G04/G05, public activation or release.
