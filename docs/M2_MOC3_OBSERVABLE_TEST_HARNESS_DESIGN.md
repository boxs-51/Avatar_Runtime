# M2 MOC3: agent-callable and visible realtime test harness

**Status:** T0/F1, bounded F2 host pixels, F3 linked physics and F4-A real core/host suite implemented and verified locally. Persistent interactive model sessions and continuous live controls remain HOLD; author/Cubism reference equivalence remains separate. **Parents:** [framework target](M2_MOC3_FRAMEWORK_TARGET_DESIGN.md), [F1 admission](M2_MOC3_F1_ADMISSION_DESIGN.md), [F2 pixel contract](M2_MOC3_F2_PIXEL_DESIGN.md). This harness is a private test application around the optional MOC3 path, not a new public `ars/ars.h` ABI or a network service. The official Live2D SDK is deferred. Local licensed models and all derived frames/results stay under ignored `build/` storage. See [T0 evidence](audits/M2_MOC3_T0_HARNESS_EVIDENCE.md) and [F2 local acceptance](audits/M2_MOC3_F2_LOCAL_ACCEPTANCE.md).

The implementation is `scripts/moc3_test_harness.py`, its local agent client is `scripts/moc3_harness_client.py`, and the visible launcher is `launch_moc3_test_harness.bat`. Run the launcher, then call `python scripts/moc3_harness_client.py status`, `create`, `step <session_id>`, `run <session_id> T0-SYNTHETIC`, `result <run_id>` or `events <run_id>`. The service writes its loopback port and token to ignored `build/moc3-harness/session.json`, deletes that descriptor on clean exit, and never prints the token. The launcher restricts the ignored descriptor to the current Windows account with `icacls`; if that fails, the service exits before publishing the token. Control sessions still accept only `synthetic_t0`. `F1-CPU-PREFLIGHT` runs generated package tests; `F1-LOCAL-REPLAY` discovers one fixture under ignored `local-assets`, checks CPU package decoding and calls the native evaluator bridge. `PASS` for `T0-SYNTHETIC` proves only its event sequence. `PIXEL-2D` now runs the actual F2 host path when its optional binaries and fixture exist; PHYS-LINK runs the bounded F3 adapter when available; continuous live suites still return typed HOLDs.

For F2, build the optional `ars_moc3_pixel_tests` and `ars_moc3_pixel_bridge` targets, then select **Run F2 pixels** or run `PIXEL-2D` through the client. The same native dual-mode host publishes completed offscreen frames to the UI and authenticated `/frames/latest`, `/frames/latest/image` and `/frames/latest/rgba` routes. The image route is a checker presenter composed after readback; RGBA is the clean target. Binary headers identify session/run/snapshot/frame/device/epoch, including when counters restart in another run. Windows Job ownership and a gated runner start ensure cancellation retires the whole process tree. This bounded replay is implemented; persistent interactive stepping, A/B inspection, live performance/soak and continuous physics controls remain future work. Availability never awards a frame or a PASS before actual execution.

For F3, select **Run F3 physics** or run suite `PHYS-LINK`; `python scripts/moc3_verify_visible_f3.py` checks the complete visible/API replay and active-render cancellation. It uses admitted manifest-linked bytes and the real native catalog, not a viewer spring. The same frame routes return clean host pixels with validated behavior labels for physics on/off, reset/reload replay and disabled output. Private detailed traces stay ignored. See [F3 design](M2_MOC3_F3_LINKED_PHYSICS_DESIGN.md) and [local proof](audits/M2_MOC3_F3_LOCAL_ACCEPTANCE.md). Whole-package interactive reload, persistent stepping and reference equivalence remain HOLD.

For F4-A, select **Run F4 core** or run `CORE-INTEGRATION`; `python scripts/moc3_verify_visible_f4.py` verifies the mapped window and complete API event stream. The native worker uses actual Character/source authority and semantic publication, then linked physics and the host target. Packets own copied bytes from a two-slot native ring and include core generation; binary responses expose `X-Core-Generation`. A rejected private target after late revoke is not advertised. This is a bounded private consumer, with [local evidence](audits/M2_MOC3_F4_LOCAL_ACCEPTANCE.md) and a separate [public-product HOLD ledger](M2_MOC3_F4_RELEASE_LEDGER.md), not a public installed SDK or persistent external control session.

For the current local diagnostic, build `ars_moc3_harness_bridge` with `ARS_BUILD_MOC3_INDEPENDENT=ON` and the locally pinned Purism source, place exactly one authorized `.model3.json` under ignored `local-assets`, then launch `launch_moc3_test_harness.bat`. The window's **Run F1 local** button and `python scripts/moc3_harness_client.py run <session_id> F1-LOCAL-REPLAY` invoke the same suite. `result <run_id>` and `events <run_id>` expose the coarse PASS/FAIL/HOLD and timeline without private paths, hashes or image data. The suite exposes PACKAGE, EVALUATOR, NEGATIVE, MEMORY and COMPLETE stages on the same API/window timeline. Its proof tier is `trusted_local_cpu`; the actual local model run is the primary F1 evidence, and GitHub Actions is corroborating. See [local acceptance](audits/M2_MOC3_F1_LOCAL_ACCEPTANCE.md). The synthetic control-session endpoints do not retain this worker after the run; persistent host model sessions remain explicitly HOLD.

## Purpose and two synchronized clients

The owner needs to observe realtime behavior in a desktop window similar to the current local interactive launcher, while an agent invokes tests and receives immediate machine-readable results. One **host-owned test session** is the source of truth for both clients:

```text
fixture alias + scenario -> test coordinator -> admitted package/evaluator -> committed snapshot
                                       |                                |
                         versioned loopback API              host D3D11 offscreen pass
                                       |                                |
                                agent JSON/events      clean readback + desktop presenter
                                                                    |
                                                    operator UI: image, controls, timeline
```

The API and visible window report the same `session_id`, `run_id`, `frame_generation`, `target_epoch`, simulation timestamp and case verdict. The desktop presenter may add a HUD **after** clean offscreen readback; test pixels never include FPS text, buttons or cursor. If F2 has not supplied the host renderer, the UI may show evaluator geometry/status, but must label host pixels `HOLD_NO_HOST_RENDERER`. An external viewer screenshot cannot silently replace them.

The current untracked `launch_interactive_viewer.bat` starts a separate viewer against a fixed local model. The related untracked scripts use still-shot `--setparam` calls, externally chosen pixel-difference thresholds and, in one timeline, an independently coded spring equation. Those paths can provide a human comparison but do **not** prove the host renderer or linked Physics3. The new launcher should accept a **locally configured fixture alias**, open the host-owned test window and expose the session endpoint; it must not copy a private model path, frames or derived metrics into tracked files.

## Private API contract

Use versioned HTTP on `127.0.0.1` with an OS-assigned port for easy invocation by local agents and test tools. Bind no LAN interface. A per-launch random control token is stored only in a restricted, ignored local session descriptor; never print it, put it in URLs, or include it in reports. All routes except a minimal readiness probe require the token. Accept fixture aliases from a preconfigured allowlist, never an arbitrary client-supplied filesystem path. This is a local test-control boundary, **not** public asset admission or authorization for an exposed service. A CLI client may wrap the same protocol for shell-only agents.

| Route | Request / response | Observable guarantee |
| --- | --- | --- |
| `GET /v1/status` | Current build revision, backend (`CPU`, `WARP`, named GPU), feature support, active session/run, UI state and typed HOLDs. | Says `UNAVAILABLE` for stages not implemented; cannot report green from launcher readiness alone. |
| `POST /v1/sessions` | `{fixture_alias, mode: "deterministic"|"live", viewport_profile}` -> session ID, capability report. | Full F1 admission or typed rejection; no partially active model. |
| `POST /v1/sessions/{id}/runs` | `{suite_id, case_ids?, clock_profile}` -> run ID and initial `QUEUED` state. | One run owns stimulus/control; returns immediately, supports bounded progress polling. |
| `GET /v1/runs/{id}` | Stage/case progress, final verdict, evidence tier, error codes, result summary. | `PASS`, `FAIL`, `HOLD`, `UNVERIFIED`, `CANCELLED` are distinct. Every final verdict includes its direct oracle. |
| `GET /v1/runs/{id}/events?after=seq` | Bounded ordered events with next sequence/cursor and overflow marker. | Agent can follow the same timeline as UI without polling raw screenshots. Cursor gap invalidates a continuity claim. |
| `POST /v1/sessions/{id}/steps` | Deterministic mode only: complete parameter frame or named stimulus plus exact `delta_ns`/step count. | Returns applied input, output parameter digest, snapshot generation, frame generation and typed step result after commit; never returns before the claimed frame exists. |
| `POST /v1/sessions/{id}/actions` | Live mode only: semantic input, pointer event, physics toggle, pause/resume/reset; one active control lease. | Records accepted sequence/timestamp and later observed frame; a request ack is not a physics/render PASS. |
| `GET /v1/sessions/{id}/frames/latest` | Clean offscreen frame metadata and optional bounded PNG/readback via separate binary response. | Generation/epoch match a completed host submission; stale or pending readback returns `BUSY`/typed error, never an old frame under a new ID. |
| `POST /v1/runs/{id}/cancel` | Idempotent cancellation request. | Stops new stimuli, then drains/retains in-flight pins according to host query rules. |

All POST requests carry a client request ID; duplicate IDs return the same admitted result rather than applying input twice. Include schema version, monotonic event sequence, bounded request/body size, timeout and rate limits. Reject nonfinite/out-of-range values, unknown parameter or case IDs, invalid clock deltas, wrong mode, stale session/generation and concurrent controllers with typed errors **before** mutating evaluator or D3D state. The UI has an operator control lease; an agent run may acquire it while the UI remains viewable. Manual user action during an automated case pauses/cancels that case with an explicit `OPERATOR_INTERVENTION` result; it cannot be silently mixed into deterministic evidence.

Example of a **future** linked-physics result shape; the T0 endpoint currently returns HOLD for this suite:

```json
{
  "schema_version": 1,
  "run_id": "opaque-run-id",
  "case_id": "PHYS-LINK-RELEASE",
  "verdict": "HOLD",
  "reason": "LINKED_PHYSICS_NOT_IMPLEMENTED",
  "proof_tier": "host_integrated",
  "frame_generation": null,
  "target_epoch": null,
  "measurements": {},
  "artifact_refs": []
}
```

The production implementation must validate that `proof_tier` is **earned**: `viewer_diagnostic`, `evaluator_only`, `host_integrated`, `hardware_direct`, and `physical_loss_direct` are separate tiers. A missing frame or missing reference oracle is `HOLD`/`UNVERIFIED`, not a successful zero-difference comparison. The API must omit model identifiers, private paths, screenshots, raw parameter lists and model-derived counts from any public log. A local authorized client may request private frames or detailed metrics, but the exporter strips them from public CI artifacts.

## Visible realtime window

The test application should open a desktop window showing the **actual host offscreen output** when F2 exists. Provide a fitted model view; current/previous-frame A/B and mask/alpha inspection; pause, single-step and slow-motion controls; fixture/scenario selection by alias; input source and physics enabled/disabled indicators; a small timeline of accepted input, physics step, snapshot publication, GPU submission and readback; and a status panel with FPS/frame time, resource budget, stale/drop counters, case verdict and explicit HOLD reason. Keep HUD/presenter rendering separate from the clean pixel target.

Mouse drag and hover can be mapped to declared semantic inputs, but the UI must show whether motion came from direct parameter control, an authored linked Physics3 engine, or a diagnostic spring. Disabling/releasing input should visibly show the ensuing trace, not only one static screenshot. Expose a `Show oracle` view for a rights-cleared reference when one exists; otherwise label the comparison `REFERENCE_MISSING`. Keep a user-visible `Copy run ID` and local evidence export control, with a clear private-data warning before exporting model-derived images. Do not sync reports to external application directories.

The launcher first checks the optional build, local fixture alias and host renderer capability. It opens the window and writes an ignored session descriptor containing loopback port and token with restricted access. On close it revokes the token, stops admissions, drains/quarantines outstanding GPU work and exits with a meaningful code. The API should remain responsive to `status` during a long run, and window close/cancel should produce a terminal event rather than leaving agents waiting indefinitely.

## Test suites and direct oracles

| Suite and stage | Agent action / UI observation | Automatic evidence and pass boundary |
| --- | --- | --- |
| `ADMIT` (F1) | Load valid and hostile fixture aliases; UI shows graph/capability or error. | Exact admission result, zero GPU calls on rejection, unchanged prior session, bounded memory. No image PASS is claimed. |
| `POSE-REPLAY` (F1) | Step default -> two finite parameter poses -> reset -> repeat. | Full drawable geometry/UV/order/opacity comparison and generation; expected change and repeatability frozen per fixture. A visual change alone is insufficient. |
| `PIXEL-2D` (F2) | Run atlas, UV, alpha edge, order, mask/inversion and enabled blend cases; UI allows A/B. | Numeric clean offscreen readback against rights-cleared generated reference at frozen viewport/format/tolerance, WARP plus named GPU where available. Blank/readback failure is FAIL/HOLD. |
| `PHYS-LINK` (F3) | Toggle linked physics, apply impulse/semantic input, release, replay fixed timeline; UI plots actual state. | Physics3 file is manifest-linked and parsed, fixed timestamps/input-output/geometry/pixel trace, on/off and release response. Reference equivalence is a separate claim. Directly setting hair parameters or using a toy spring is `viewer_diagnostic` only. |
| `LIVE-CONTINUITY` (F2/F3) | Agent starts bounded live run; operator watches motion and event timeline. | Monotonic input/snapshot/frame generations, no event gaps, bounded frame backlog and measured timing on named hardware. Human visual review is recorded separately from automatic PASS. |
| `FAULT-LIFECYCLE` (F2/F4) | Inject path/budget/allocation/resize/close/query/device errors; UI shows prior valid or quarantined frame. | Typed error, old-state preservation where safe, exact query/resource retirement and no stale-epoch publication. Injection cannot close actual-loss D06/D08. |

For each case, capture exact source SHA, build options, fixture rights/provenance, backend/driver, clock profile, accepted inputs, snapshot and frame generations, oracle version, threshold frozen before execution, raw local event packet and final verdict. The API result is a **summary** of that packet; a human can verify the same run in the window. Private-model pixels and derived data remain in ignored local evidence. Public CI uses only redistributable inputs and sanitized outputs; when a real redistributable MOC3 fixture is unavailable, mark the full evaluator-to-host CI path HOLD.

## Phased realization and stop conditions

1. **T0 contract harness:** define schema, aliases, token/lease, event ordering and fake generated frame fixture. API/unit tests prove duplicate request behavior, cursor gap, invalid input and cancellation. Visible UI shows simulated status only and labels it as such.
2. **T1 F1 hookup:** connect admission and evaluator snapshots; agent can run `ADMIT` and `POSE-REPLAY`, UI shows parameter/geometry timeline. Host pixel and Physics3 cases remain HOLD.
3. **T2 F2 hookup:** connect the same host-owned offscreen target to clean readback, agent API and window presenter. Run `PIXEL-2D`, resize/fault cases and prove UI frame ID matches API frame ID. A separate external viewer cannot satisfy this checkpoint.
4. **T3 F3/live hookup:** connect linked Physics3 and real-time stimulus, plus deterministic replay of the same input stream. Run `PHYS-LINK` and `LIVE-CONTINUITY` while the owner can observe the window. Review clock/parameter arbitration before treating any trace as model behavior.
5. **T4 integration review:** repeat exact-head tests on WARP and a named GPU, evaluate public-input/security and feature activation separately. Physical D06/D08 remains HOLD until a disposable reset-capable runner supplies direct loss/recovery evidence.

Stop on a mismatch between API result and visible run identity, a claimed frame without completed readback, a hidden toy-physics substitution, leaked private data, or a test that derives its threshold from the same output it judges. In each case report `FAIL` or `HOLD` with the first failing boundary; do not infer success from a responsive window, HTTP 200, changed pixels or green CI alone.

## F4 persistent trusted-local follow-on

`CORE-LIVE` keeps one native core/evaluator/host worker and exposes authenticated `POST /v1/runs/{run_id}/controls`. Body: request_id, increasing integer sequence, normalized drive (0..1 or null release), physics/reset booleans and lease_ms (100..2000). HTTP 202 means QUEUED; CONTROL_COMMITTED binds the accepted sequence to an actual jointly published frame. One pending command; stale/wrong/terminal/busy input rejects. An expired command releases manual input against the worker monotonic wall clock. The alias hides private native identifiers. This is a local host-source test API, not the public model-loading SDK.

The window adds Start core live, two-second Drive/Physics off, Release and Reset controls. `CORE-SOAK` runs five minutes warm-up plus thirty measured minutes at a requested four presentations/second; simulation advances six fixed physics steps per presentation, not elapsed-wall catch-up. UI/API share the same immutable frame bytes and core identity. Short live API identity and long continuity are distinct proofs; terminal soak UI capture was missed after auto-close and is not claimed.

Public-user input was selected subsequently. Neither CORE-LIVE nor the installed trusted-local consumer accepts arbitrary public paths. The AppContainer prototype proves bounded MOC parser preflight only; [full public pipeline work](M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md) must finish before connecting user uploads.
