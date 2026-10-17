# M1-D next implementation design: close, retirement and device loss

**Status:** design candidate for the next private D3D11 code increment, based on PR #30 source `4efbe25cc99ab75fb64f3e23e34d9e53c5910a59`. This document is a work plan, not an implementation result, G00 approval, M1-D PASS, public API change or merge authorization. The current per-case verdict remains `PRIVATE_SPIKE_PARTIAL; M1-D_ACCEPTANCE_HOLD; PUBLIC_RENDER_DISABLED`.

**Implementation follow-up (2026-10-17):** Private source `f258812791a666bca9985a3966e062ccdf9f4e49` implements the close coordinator, terminal query paths and injected loss quarantine, including pre-draw retirement capacity reservation. The exact-source results and updated HOLD decisions are in `audits/M1_D_GATES_REASSESSMENT_20261017.md`; the remainder of this document records the pre-implementation design and is not retrospective PASS evidence.

## What the existing spike proves and where it stops

The private Windows opt-in host backend draws and restores the pipeline state it touches. An event query retires each of 100 recreated targets; pins remain held until the exact query completes. The D3D11 debug layer reported the same seven live-object identities before and after that loop. Shared/static CTest passed 31/31 and 29/29 on the named GTX 1060 runner; 135 Python contracts and exact-head M1-D CI passed. These results do not cover a close request while work is pending, post-close COM lifetime, failed `GetData`, actual device removal/reset, or concurrent host close/resize. The public render/drain ABI and capability remain disabled.

Current source gap: `QueryRetirement::~QueryRetirement` terminates if an object is destroyed before query completion. A failed `GetData` or removed device sets `quarantined_`, but no owner-thread recovery or explicit release path exists. `HostDevice` has no close/generation transition. The next increment must make these paths observable and bounded without treating `Flush`, CPU return from `Draw`, or an error as GPU completion.

## Ownership and lifecycle contract for the next private increment

The host owns the device, immediate context and render target. It must serialize all immediate-context calls on one graphics R thread and keep borrowed resources valid until the agreed close handoff. The SDK/private backend holds its own COM references and immutable snapshot pins for outstanding work. `begin_close` may be requested from another thread but only records a close intent; `poll`, `Flush`, state restore, resource release and final detach run on R. No host callback runs under a graphics or source lock.

Use an explicit per-generation state machine, with transitions recorded in test-visible diagnostics:

| State | Allowed operation and result | Retained ownership |
| --- | --- | --- |
| `ACTIVE` | Validate, submit and create one exact event query per submitted draw; generation/device mismatch rejects before state mutation. | Host device/context plus SDK-owned resources and snapshot pins for each pending submission. |
| `CLOSING` | Reject new submissions; R thread polls pending queries once per drain call and returns `BUSY` while any query is pending. A single R-thread `Flush` may make queued work progress; it is not completion. | All pending pins and COM references remain held. Host cannot release its borrowed device/context/target yet. |
| `DRAINED` | Enter only after every exact query for that generation returns `S_OK` with true completion data. Destroy completed retirement entries and release their device/context references, pins and backend-owned shader/state caches on R; report close complete and allow host handoff. | No outstanding SDK GPU references for the retired generation. |
| `LOST_QUARANTINED` | Enter when `GetDeviceRemovedReason` fails or an exact query returns a terminal error. Reject submit and never report query completion. Preserve failure reason and generation. | Retain pins/references until a separately acknowledged lost-device teardown barrier; do not use the old device again. |
| `DETACHED_LOST` | After host acknowledges it has stopped all use of the old device/context and an R-thread teardown barrier executes, release SDK-held references and pins without labeling the query completed. A replacement device requires a new generation and fresh GPU resources; immutable CPU assets/pose stay valid. | No SDK references to the old device generation. |

The handoff after loss is an explicit **teardown** outcome, distinct from `DRAINED`. The exact host acknowledgement/barrier API is private in this increment and must be reviewed before coding: it must prevent another thread from using the old immediate context, establish ordering before COM `Release`, and be callable only once per generation on R. `S_OK` with false event data is not completion and must enter a diagnosed terminal failure state. A timeout while the device is still healthy cannot authorize release; it leaves `CLOSING`/`BUSY` and an intact host lifetime obligation. No destructor should be the normal pending-work control path. If a caller violates the lifetime contract, fail closed with a diagnosable status/invariant instead of silently freeing in-flight resources. The test harness must not call `ClearState` on a borrowed host context merely to make leak counts look clean; only host-owned teardown may do so after handoff.

## Implementation order and acceptance checks

1. **D07 close and generation handoff.** Add a private close coordinator around `HostDevice` and a collection of `QueryRetirement` entries. Make admission/close atomic with respect to new submissions; move all D3D calls to R. Specify idempotent `begin_close`, bounded `poll_close` and the exact final host-release acknowledgement. RED-first tests cover close before first submit, after a completed submit, with a query still `S_FALSE`, repeated close, wrong thread, stale generation, resize/device replacement during close and concurrent close-versus-submit. A healthy pending query must return `BUSY`, retain pins and leave the host handoff unavailable. A completed query must release references once on R. Record 100 resize/close cycles and a post-close live-object report with expected host/debug references excluded and zero SDK-owned survivors.
2. **D05/D08 terminal failure and quarantine.** Isolate the narrow query/device-status calls behind a private injectable interface, while production still calls D3D11 directly. RED-first deterministic tests inject `S_FALSE`, `S_OK`, failed `GetData`, and removed/reset reasons at each transition. Verify the exact query/generation is the only success path; `S_FALSE` stays `BUSY`; failure enters `LOST_QUARANTINED` with no pin release or false completion. Test R-thread teardown acknowledgement, repeat acknowledgement, wrong-thread rejection, new-device/new-generation recovery and old-resource rejection. Preserve CPU asset and logical pose. An injected HRESULT proves control-flow behavior only, not physical device loss.
3. **D02/D03/D06 and physical repeat.** Add stale target lifetime and concurrent host serialization tests without dereferencing a released raw pointer. Use a retained old RTV plus invalidated generation, a second device, and controlled close/submit races. Compare all touched host state after success and recoverable failure, including dynamic shader class instances if this feature is to be supported; otherwise explicitly reject/hold that case. Repeat the physical readback, event-query, 100-cycle resize/close and debug-layer live-object reports on a named runner. Capture a report after close and after releasing host-held references so SDK-owned and host-owned lifetime are not conflated.
4. **Actual device removal/reset evidence.** Run only in a disposable, dedicated Windows GPU environment with an operational recovery path. Capture the actual `GetDeviceRemovedReason`/DXGI result, transition log, host teardown acknowledgement, new-device recovery, post-teardown live-object report, OS/driver/GPU and raw log hashes at the exact tested source SHA. Do not induce a TDR on a shared workstation or claim that a synthetic HRESULT is a physical pass. Until this experiment exists, D08 remains HOLD even if the injected tests pass.

Each step is a separate reviewable source/evidence commit. Keep `src/graphics/d3d11/**` and `tests/gpu/**` pinned to the exact tested source SHA in the M1-D custody gate; update the evidence ledger only after the relevant source commit and physical run. Retain shared/static Windows CTest, Python custody/contract tests, Linux opt-in guard and CI. No public `include/ars/ars.h` or `src/abi/**` change belongs to this increment.

## HOLD disposition and decision boundary

| Case/gate | Evidence that can clear it | Status until then |
| --- | --- | --- |
| D00 / G00 technical-method | RED-first review of the lifecycle state machine, ownership proof, cleanup/error mapping and exact candidate SHA under ARS-SOLO-OWNER-001; record reviewer identity and limits without fabricating actor-distinct approval. | HOLD; owner design YES alone is not technical-method PASS. |
| D02/D03 | Valid retained stale target/generation, wrong-device and bounded close/submit concurrency tests on the R-thread contract. | PARTIAL. |
| D05 | Deterministic failed-query injection plus physical success path; no error mapped to completed. | PARTIAL. |
| D06 | Pins held through `BUSY` and loss quarantine, then released at proved completion or acknowledged teardown; post-close debug live-object count. | PARTIAL. |
| D07 | 100 resize/close cycles, pending-close `BUSY`, exact drain/handoff, wrong-thread and post-close leak evidence. | HOLD. |
| D08 | Injected failure matrix **and** separately named actual device removal/reset run with recovery and teardown evidence. | HOLD. |
| D09/D10 | Exact-head matrix, full raw logs/hashes, source-to-evidence pin and independent review of the missing cases. | PARTIAL. |
| Full M1-D/G02/B/G03-G05 | Separate gate evaluations after all prerequisites; no automatic promotion from a private spike or passing CI. | HOLD. |

The next implementation can start with steps 1 and 2 after this design is reviewed. Do not mark D07/D08 PASS based on a unit mock, normal GPU completion loop or CI without a physical GPU. Do not merge or activate public rendering solely from this plan.

## External API facts used by this design

- Microsoft documents `GetData` `S_OK` as available and `S_FALSE` as not yet available; event query data comes from the immediate context: https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata
- `GetDeviceRemovedReason` exposes removed/reset/hung and related DXGI failure reasons: https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-getdeviceremovedreason
- Microsoft directs applications to recreate device-dependent resources after removal/reset; its optional force-TDR test is reserved here for a dedicated runner: https://learn.microsoft.com/en-us/windows/uwp/gaming/handling-device-lost-scenarios
- `ReportLiveDeviceObjects` reports device-object lifetime information; its output is evidence only when the report boundary and held host references are recorded: https://learn.microsoft.com/en-us/windows/win32/api/d3d11sdklayers/nf-d3d11sdklayers-id3d11debug-reportlivedeviceobjects
