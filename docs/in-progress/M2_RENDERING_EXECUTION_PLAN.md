# M2 rendering execution plan — 2026-10-17

**Base:** draft M1-D PR #30 at `dffe10c16d5585c0d50fd7745f310543e719c522`. **Decision:** begin a separately reviewable M2 stack; do not merge M1-D or advertise public rendering from this plan. The owner directed the unexecuted physical D06 post-loss lifetime and D08 actual removal/recovery experiments to remain **HOLD** in the M2 graphics backlog. No reset-capable runner is available.

M2 in `ROADMAP_AND_ADRS_V0.1.md` covers native 2D, GLB 3D, offscreen composition and atomic dual-mode switching. The relevant acceptance gates are G03, G04 and G05. Full G00 technical-method, G01 public runtime and G02 public asset integration are still HOLD, so each source increment below is private until those dependencies are demonstrated. M3 motion and M4 release are outside M2.

| Increment | Source and behavior | Direct checks | Gate boundary |
| --- | --- | --- | --- |
| M2-A static 2D draw preparation | Consume only a validated immutable CPU model; select visible parts; preserve effective ancestor visibility/opacity from the validator; order by layer z/declaration then part z/declaration; copy bounded base mesh and UV data into a prepared snapshot. | Native Windows/Linux test with synthetic ordering, hidden/opacity, invalid index, budget failure and real accepted ARSCHAR. Output unchanged on failure; no GPU calls. | Private CPU foundation; G03 HOLD until pixels are rendered. Deformation and dynamic layer overrides deferred. |
| M2-B 2D D3D11 offscreen | Define and review UV/atlas sampling, blend/color-space and pixel-center rules; upload prepared 2D geometry/atlas to the host device on R; restore all touched state; retire resources with exact query completion. | Golden pixel/readback for order, alpha, hide/show, resize and failure; debug-layer resource report on the named GPU. | G03 only after integrated host behavior. D06/D08 remain HOLD and block any device-loss/recovery claim. |
| M2-C 3D GLB subset | Consume C2 validated GLB mesh, hierarchy, skin, morph and base material; define camera/light and resource budgets before GPU upload. | Golden 3D readback including skin/morph and invalid-resource rollback. | G04 HOLD until real 3D output; no VRM or animation capability implied. |
| M2-D dual-mode transaction | Prepare candidate representation and resources before commit; retain prior mode and pose on unsupported mode or allocation failure; retire old GPU generation safely. | 100 switches, missing-mode rejection and injected OOM rollback with unchanged prior frame/pose/resources. | G05 HOLD until both G03 and G04 have direct integrated evidence. |

M2-A adds an opt-in private library and CTest. It does not change `include/ars/ars.h`, `src/abi/**`, security/source authority, schemas, fixture bytes or the M1-D evidence ledger. Source and logs must be pinned to an exact M2 SHA on a draft PR. Later increments require their own reviewable commits, gate reports and any material ADR decisions; CI success alone cannot promote G03/G04/G05.

## Physical-loss carry-forward

`D06_POST_LOSS_LIFETIME` and `D08_ACTUAL_DEVICE_LOSS` remain HOLD as recorded in `M1_D_GPU_CASE_EXECUTION.json`. When a disposable Windows GPU runner with driver-reset recovery exists, record the actual DXGI reason, R-thread quarantine and host teardown barrier, replacement-device recovery, post-host-release live-object report, exact source SHA and raw log hashes. No TDR is induced on the shared workstation. Their schedule does not turn an injected WARP result into physical PASS.
