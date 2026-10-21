# M2-E host handoff and remaining rendering acceptance

**Base:** PR #37 HEAD `ef28b2f7d12c3b993a4a2fbc80eca8426ff68fc3`. This branch continues the private M2 work while #37 waits for CI and merge. It does not change the public ABI, advertise rendering or promote a gate. The M1-D case ledger remains authoritative.

| Step | Work and exit evidence | Limit |
| --- | --- | --- |
| E0 — private teardown receipt (this branch) | Replace `host_quiesced` in `CloseCoordinator` with a single-use receipt from an isolated host that has blocked admission, drained its tracked context users and surrendered its own device/context/RTV references. WARP RED controls reject missing, early, wrong-thread, wrong-generation, wrong-host and repeated receipts while retaining the quarantined pin. Verify a valid receipt releases it on R. | This source owns only the private test host's references. It cannot prove that every production immediate-context caller has stopped. G00 technical-method and D06/D08 remain HOLD. |
| E1 — production host handoff | Inventory every immediate-context caller and committed pose/camera producer. Route them through one generation-bound admission fence. On R, stop new work, drain or invalidate target users, surrender the production device/RTV references, then issue the same receipt. Run resize/close races with exact resource identities and a source-pinned debug-layer report. | D02/D03 `HOLD_HOST_INTEGRATION` and G00 method can only be reassessed after direct integrated evidence and exact-head review; unrelated G00 ADR/security work remains open. |
| E2 — integrated render acceptance | Bind validated immutable 2D and 3D snapshots to committed host pose/camera. Run dynamic 2D visibility/opacity/Z and resize pixels, 3D fractional LOD/mip/minification pixels, allocation rollback, 100 switches and before/after live-object comparisons on WARP and named hardware. | G03/G04/G05 remain HOLD until their full integrated matrix passes. G01/G02 public runtime and asset activation are separate prerequisites. |
| E3 — physical loss packet | On a disposable Windows GPU runner, observe an actual DXGI removal/reset, quarantine old generation, complete the production host barrier, recover with a new device and compare post-release live objects against the exact source baseline. Record adapter/driver, raw logs and hashes. | D06/D08 physical portions stay `HOLD_CARRY_TO_M2` while no reset-capable runner exists. No TDR or driver reset on the shared workstation. |

E0's negative controls must fail before the receipt exists and must not release a quarantined CPU pin. A receipt is valid only for the coordinator registered with that isolated host and device generation. Successful WARP injection is protocol evidence, not physical loss/recovery evidence or a gate PASS. Review the final E0 source and logs separately from the pending #37 merge before using them to amend any HOLD ledger.

## Current #38 E1 checkpoint - 2026-10-21

After #59/#60, [the owned native design](ALPHA_38_PRODUCTION_OWNERSHIP_DESIGN.md)
is implemented in the actual packet/lease adapters and 2D/3D/MOC3 host graph.
[Direct local acceptance](../completed/audits/ALPHA_38_PRODUCTION_OWNERSHIP_ACCEPTANCE.md)
records canonical admission, registered GPU owners, readback/restoration/caches,
real runtime pins, two-phase receipts, healthy/injected WARP and named hardware
and exact live-object identities. G00 technical-method is accepted only for this
registered native graph. Public graphics and unregistered future SDK users are
not certified; whole G00 remains HOLD. E2/#39 committed pose/camera/render matrix
and E3/#40 actual destructive loss remain separate. Earlier E0/E1 table entries
describe the original plan, not current absence of this implementation.
