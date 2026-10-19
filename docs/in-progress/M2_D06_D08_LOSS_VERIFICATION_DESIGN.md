# M2 D06/D08 loss-verification design — 2026-10-17

**Status:** test-method design; both physical verdicts remain `HOLD_CARRY_TO_M2`. This companion to `M2_C_GLB_RENDER_DESIGN.md` separates safe local fault injection from the direct device-loss experiment. No test in this design authorizes driver reset on the shared Windows workstation.

## Evidence already available and its limit

`tests/gpu/d3d11_close_protocol.cpp` uses WARP plus a private `QueryOps` script to return `DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_RESET`, failed `GetData` and BUSY/completion outcomes. It checks quarantine, pin retention, R-thread acknowledgement and a replacement device. The healthy physical `d3d11_live_capture` path checks 100 retirement/close cycles and matching before/after live-object identities. Both ran again locally on the M2-B stack; they prove code-path behavior and healthy lifetime, **not** actual post-loss behavior. The ledger in `M1_D_GPU_CASE_EXECUTION.json` remains authoritative for the HOLD statuses.

The M2-B 2D renderer calls the actual `HostDevice::validate()` and `GetDeviceRemovedReason()` directly. Its current WARP pixel test does not inject loss at the 2D draw boundary. A small private fault seam is needed before claiming coverage of the integrated M2 render/retirement path; it must default to native Direct3D calls and never be exposed in the public ABI.

## Safe local simulation to implement next

Use a dedicated test process with `D3D_DRIVER_TYPE_WARP`, no TDR command, no driver controls and no registry changes. Add an injectable private device-status/query interface shared by host validation, post-draw loss check and query retirement. Test each failure point separately: before admission; after resources are prepared but before target mutation; after submitted draw; first and repeated `GetData`; and close/teardown acknowledgement. A fake result must not fabricate a physical removed device: label logs `INJECTED_WARP` and record the actual device's `GetDeviceRemovedReason()` separately.

| Safe case | Required assertions |
| --- | --- |
| Pre-admission removed/reset | Return `device_lost`; target pixels and host pipeline unchanged; no GPU bundle or query orphan. |
| Post-submit removed/reset | Quarantine exact generation and retain target, GPU bundle and immutable model pin; reject subsequent draw and old-generation target. No normal `poll()` may declare completion. |
| `GetData` BUSY then failure | BUSY keeps pins; failed terminal status quarantines. Do not treat `S_OK` with `FALSE` event data as completion. |
| Teardown barrier negative control | Wrong thread, wrong generation, still-active host context user, missing barrier and repeated acknowledgement all fail without early pin release. |
| Replacement device | New generation accepts only new-device RTV/resources; old RTV and old generation fail before mutation. A healthy new readback succeeds. |
| Cleanup | After verified acknowledgement, release M2 owned objects on R; compare debug-layer live-object identities before/after the isolated test. Mark this `INJECTED_CLEANUP`, not physical D06 PASS. |

`CloseCoordinator::acknowledge_lost_teardown(generation, bool host_quiesced)` currently trusts a caller-provided Boolean. For an integrated host test, replace or wrap this with a generation-bound teardown receipt issued only after the host has stopped all immediate-context callers, drained/invalidated in-flight target use and surrendered its old device/RTV references. Test a forged/stale receipt as a RED control before accepting a successful one. This is a G00 technical-method dependency as well as D06/D08 work; a fake Boolean alone cannot establish the barrier.

## Future direct physical experiment

Run only on a disposable Windows GPU runner with out-of-band recovery. Pin the exact source SHA, GPU/driver/OS, debug-layer state and pre-loss live-object baseline. Start an M2 2D or 3D query-retained submission, induce a real loss under runner control, capture the actual `GetDeviceRemovedReason()` from the old device, and record admission stop, R-thread quarantine, host teardown barrier, release order and refusal of old resources. Create a new device and target, render/read back a replacement frame, then release host references and capture the post-release live-object report. Compare SDK-owned identities against baseline, not just total warnings. Save raw logs and hashes. The test must distinguish `DEVICE_REMOVED`, `DEVICE_RESET` and any other observed HRESULT without rewriting it into the expected one.

**D06 PASS condition:** after a real loss and proved host teardown, no SDK-owned old-generation resource survives host release; pins are retained until that barrier, with source-pinned debug evidence. **D08 PASS condition:** real DXGI loss/reset reason, quarantine, old-resource rejection and successful replacement-device recovery are directly observed. If the runner cannot induce or recover from loss, keep the respective case HOLD. A successful WARP injection, ordinary GPU readback, healthy close or CI job never changes those physical verdicts. Microsoft's [D3D11 device-loss guidance](https://learn.microsoft.com/en-us/windows/uwp/gaming/handling-device-lost-scenarios) treats `-forcetdr` as a forced GPU timeout and requires recreation of device-dependent resources after actual removal.
