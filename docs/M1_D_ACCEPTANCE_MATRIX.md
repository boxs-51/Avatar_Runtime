# M1-D proposed acceptance matrix

State: NOT_EXECUTED / PREPARATION_ONLY. Baseline `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. Every native result must name the exact source SHA, OS, GPU, driver, D3D feature level, command, expected/actual result, exit status, raw log and artifact digest. A compiler-only pass never counts as physical GPU PASS.

| ID | Required physical or contract evidence | Failure/HOLD condition |
| --- | --- | --- |
| D00 | ADR-0001 technical-method disposition, exact pathset and frozen ABI20/header/ADR pins | Material ownership, thread or query rule unresolved |
| D01 | Host-created D3D11 device/context/RTV and minimal offscreen readback on a named physical Windows GPU | No physical readback or mismatched color/alpha |
| D02 | Reject wrong-device, stale generation, null/invalid target, unsupported format/sample count before state mutation | Incorrect status, host state changed or resource leaked |
| D03 | Enforce designated R-thread and serialized immediate-context use; reject concurrent/wrong-thread entry | Cross-thread D3D11 use or blocking under runtime/security locks |
| D04 | Byte/identity comparison of all touched IA, VS/PS, RS and OM host states before/after success and recoverable failure | Any state drift or untracked touched slot |
| D05 | `End(event_query)` and bounded `GetData` progress: DONOTFLUSH then explicit R-thread progress, PENDING/BUSY then exact completion | CPU return/Flush mistaken for completion, busy-wait or wrong-query completion |
| D06 | Hold GPU COM refs, work pins and snapshots until completion; release on R thread, with live-object/refcount evidence | Early release, orphaned resource or permanent pin |
| D07 | 100 resize/close cycles with in-flight work and target invalidation | Stale target use, leak growth or failed drain/close |
| D08 | Device removal/reset and failed query paths; CPU model/pose survives while GPU generation is quarantined/recreated | Synthetic COMPLETED, stale GPU reuse or partial publication |
| D09 | Exact-source Windows MSVC shared/static and historical ABI20/CPU/security regressions; Linux guarded compile | Public export/layout drift or unsupported platform advertises GPU |
| D10 | Raw artifact manifest: source/tree, hardware/OS/driver, test commands, per-case expected/actual/exit, readbacks, state diff, memory/live objects and hashes | Aggregate-only totals, missing physical evidence or unverified artifact |

The local GTX 1060 is a candidate, not a D01-D08 PASS. Where the D3D debug layer/live-object reporter is unavailable, the leak claim stays HOLD until an equivalent measurable witness is accepted. G02, G07/G08, G03-G05 and public feature bits retain their separate gate states.
