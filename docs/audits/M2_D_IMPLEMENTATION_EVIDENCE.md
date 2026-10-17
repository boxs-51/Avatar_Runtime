# M2-D private dual-mode implementation evidence — 2026-10-18

**Tested source:** `0b7719326a684d29ac9aa2d8ec8b040f33e8f6ca`, stacked on pre-D HOLD audit `d96ff59ba0642308f0b656709fbccb511d142a93`. This report and the custody checks are evidence-only additions after that source commit. The change adds a **private** offscreen D3D11 host transaction. No public ABI, advertised graphics capability, source authority, or M1-D case verdict changed. Runner: Windows 10 x64, VS 18 / MSVC 19.51, SDK 10.0.26100, D3D11 debug layer; WARP `1414:008c` and physical NVIDIA `10de:1c02` (GTX 1060 3GB). Driver version was not available to this process through WMI, so the PCI IDs and debug output are the adapter evidence.

All six commands below exited **0** on the tested source. The raw files are retained beside this report.

| Evidence | Command / observation | SHA-256 |
| --- | --- | --- |
| [Build](M2_D_BUILD_RAW.txt) | `cmake --build build/m2-d-private --config Release --target ars_m2_dual_pixels ars_m2_2d_pixels ars_m2_3d_pixels ars_d3d11_live_capture` | `9EFC096CD82A33A900F0ABB26F294AE7E8FE503ABD8214564DD65094EFC21C9A` |
| [CTest](M2_D_CTEST_RAW.txt) | `ctest --test-dir build/m2-d-private -C Release -R '^(c2_.*\|m2_.*\|d7_warp_close_protocol)$' --output-on-failure`: 9/9 | `526E450F67CF782FF981145B76B546B3041386CEFAE585439BDDDECFB9D391B4` |
| [WARP](M2_D_WARP_RAW.txt) | `ars_m2_dual_pixels.exe <schema-worker> <asset-corpus>` | `C0CAD5E9517F67D6D24B9D5103313BDCCCF83D4E739DADEB025D326F53A02DD9` |
| [Hardware](M2_D_HARDWARE_RAW.txt) | Same executable and inputs with `--hardware` | `921C6D77624359CB791571642B216710EEB9753EE607859843C9E6E8DCAA5B6E` |
| [WARP live objects](M2_D_LIVE_WARP_RAW.txt) | `ars_d3d11_live_capture.exe --m2d <schema-worker> <asset-corpus>`: 7 identities before and after | `0B2F73A111AC2BF23DA3B067084179AF63ACED901FBF283420A2224C8DA74694` |
| [Hardware live objects](M2_D_LIVE_HARDWARE_RAW.txt) | Same debugger capture with `--m2d-hardware`: 7 identities before and after | `9B4270689B12A1937CBC687D5A18953CB5B95A794FC75832280DEF2FFF9FC7C1` |

The direct tests ingest the generated `positive-dual-full.arschar` through the schema worker, prepare owned 2D and PMR-backed 3D snapshots, and read back `(255,0,0,255)` for 2D and `(75,0,0,255)` for 3D at pixel `(2,2)` on both adapters. A candidate is drawn into a separate target and its mode, pose generation, and target epoch publish together only after its exact event query completes. Both adapters pass 100 switches, 100 resizes with stale RTV/epoch refusal, 100 deterministic off-thread close-intent races, dynamic 2D opacity and hide, missing-mode and invalid-camera rejection, target and 3D GPU-create OOM rollback with the previous frame/pose unchanged, wrong-thread refusal, and injected query removal with retained CPU pin until the isolated host detaches. The debug queue has no error or corruption messages. The live-object comparison covers healthy lifecycle only; the debugger's baseline warnings are present on both sides.

## Reassessment after the pre-D HOLD work

| Item | Current disposition and limit |
| --- | --- |
| D02 / D03 | **Private host subcases evidenced** for 100 stale-target resize refusals and 100 serialized close races on WARP and GTX 1060. **HOLD_HOST_INTEGRATION** remains for all production immediate-context callers and production host resize/close handoff. The M1-D case ledger is unchanged. |
| G03 / G04 / G05 | **HOLD.** The private transaction proves representative 2D/3D pixels, owned snapshots, atomic mode/pose publication and rollback. It does not connect the production committed-pose/camera producer, demonstrate dynamic Z and the larger fractional-LOD/mip-chain matrix, or complete public/integrated host acceptance. G05 cannot be promoted while G03/G04 are open. |
| G00 technical-method | **HOLD.** The private isolated host can close its own context users and reject wrong-thread or early loss detach, but `CloseCoordinator::acknowledge_lost_teardown(generation,bool)` still has a caller Boolean. There is no externally issued generation-bound host teardown receipt, forged/stale/early receipt RED controls, or independent exact-head method disposition. |
| D06 / D08 | **HOLD_CARRY_TO_M2.** Injected removal and healthy D3D live-object equality do not establish actual device loss, post-loss host teardown or recovery. The shared workstation has no dedicated reset-capable GPU runner; no TDR was induced. |
| G01 / G02; M1-B G07 / G08 | **HOLD.** Public runtime/capability and activation/security cases were not implemented by this private PR. D10 remains partial until direct physical-loss and method-review evidence exists. |

The next acceptance step is a separately reviewed production host handoff and generation-bound teardown receipt with negative controls; then run the full integrated gate cases on that exact source. Physical D06/D08 require the disposable reset-capable runner specified in the loss-verification design. Neither CI success nor merging this private stack changes those verdicts.
