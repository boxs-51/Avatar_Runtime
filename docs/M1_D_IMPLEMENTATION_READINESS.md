# M1-D preparation: private D3D11 HOST_DEVICE interop

State: PREPARATION_ONLY. Baseline `main` is `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc` (tree `d9cef07642959cce3788daa18bd96505c32d4055`). This packet proposes a source scope and physical evidence plan; it does not authorize GPU implementation, public runtime activation or a merge.

## Current handoff

PR27 and PR28 were merged in order as MERGE_COMMIT `dfb91f2a4c1dfb7f26d14ca7273dd431b165e01d` and `f5b198137f6a96a2e33bb8ac206e53c3815bbbbc`. The C1/C2 CPU asset components have exact-source evidence, while full G02 remains HOLD. The original 87 B whole-case verdicts remain HOLD/PASS 0. M1-D may gather private GPU evidence without changing either verdict.

Reuse the frozen ABI20 `ars_render_target` descriptor, runtime generation/close/lease primitives, immutable CPU assets and owner-selected ADR-0001 HOST_DEVICE + D3D11_QUERY_EVENT design. Preserve their source and blob identities in the preparation manifest. The repository currently has no `src/graphics` or `tests/gpu` implementation; `ars_runtime_render` and `ars_runtime_drain_gpu` still return UNSUPPORTED. ADR-0001 technical-method review and physical M1-D GPU proof remain HOLD.

A local Windows inventory reports NVIDIA GeForce GTX 1060 3GB, driver 32.0.15.8157. This is a candidate physical runner only: D3D11 creation, feature level, debug-layer availability, driver/OS identity and controlled test ownership must be measured at kickoff. Hosted CI compile success cannot substitute for physical GPU evidence.

## Proposed ordered work

**D0 — contract and runner readiness.** Pin the refreshed base, exact implementation paths and historical custody. Reconcile the touched-state restore list, borrowed device/context/RTV lifetime, R-thread serialization, target format/sample constraints and query progress/retirement with ADR-0001 and the existing C descriptor. Record a physical runner plan, failure injection method, raw artifact custody and the applicable G00 technical-method disposition. An unresolved material design conflict stays HOLD or receives a separately reviewed ADR amendment. D0 is documentation and inspection only.

**D1 — private host-device pass.** Add a Windows-only internal backend with a host-created D3D11 device, immediate context and target. Validate device identity/generation, thread, dimensions, format and sample count before state mutation. Capture and restore every touched IA/VS/PS/RS/OM state, including error exits. Render a minimal offscreen control and verify readback plus host-state equality. Fail wrong-device, wrong-thread, stale target and unsupported target cases with exact statuses. Keep the public render entry and capability bit disabled.

**D2 — completion and lifetime.** Retain needed COM references, GPU work pins and CPU/pose snapshots until the matching event query completes. Exercise `End`/`GetData`, an initial DONOTFLUSH poll, explicit R-thread progress, bounded BUSY, device removal/error quarantine and R-thread release. Prove 100 resize/close cycles, in-flight retirement, stale generation rejection and device-loss/recreation. Never call host callbacks under GPU/security locks.

**D3 — exact-source evidence and integration review.** Run the accepted D1/D2 cases on a named physical Windows GPU, retain OS/driver/feature-level/build SHA, commands, expected/actual result, raw readbacks/state snapshots, debug-layer/live-object output when available and cleanup counts. Run Windows compiler/ABI regressions and Linux compile guards separately. A missing physical runner or leak witness remains HOLD. This stage is a private GPU spike, not G03/G04/G05, G10 or release certification.

## Scope and decision boundary

The candidate implementation manifest is in `M1_D_IMPLEMENTATION_MANIFEST.json`; the case-level gate is in `M1_D_ACCEPTANCE_MATRIX.md`. Shared `CMakeLists.txt` and a new workflow are proposed explicitly because the current tree has no GPU target or GPU test job. No `include/ars/ars.h`, public ABI implementation, source-security authority, schema or frozen design blob edit is included. Any such need is a new scope decision. No SDK_DEVICE, Linux renderer, public capability advertisement, model activation or scene renderer is included.

Each implementation stage needs its own exact base/head/path and owner kickoff. Review on the actual source, then seek a separate exact-PR merge decision. Rollback is a normal reviewed revert preserving the source and evidence history.
