# ADR-0001-RC1 — D3D11 Host-Owned Device Contract (DESIGN CANDIDATE)

> **SOLO OWNER / CURRENT RC1 2026-10-14:** Owner explicitly approved Profile B layer_bindings, 87 G00 design oracles (all runtime G07/G08 HOLD), ABI20 with optional diagnostic #21 deferred, and D3D11 HOST_DEVICE/QUERY_EVENT. Historical pre-approval proposal/21-symbol/actor-distinct language below is not current authority. ARS-SOLO-OWNER-001 allows independently designed RED-first technical-method audit, not a second GitHub actor, fake approval, production certification or automatic merge. G00 design verification and owner merge decision remain separate.


**Status:** OWNER DESIGN YES; G00 TECHNICAL REVIEW HOLD; ACTOR-DISTINCT NOT RATIFIED; NATIVE GPU M1-D HOLD
**Decision owner:** sole repository owner YES, with ARS-SOLO-OWNER-001 transparent methodological review exception (not an actor-distinct approval).
**Parent contract:** docs/V0_1_CANONICAL_CONTRACT_RC1.md
**Legacy ADR:** docs/ROADMAP_AND_ADRS_V0.1.md (both HOST_DEVICE and SDK_DEVICE were proposed)
**Stage split:** G00 acceptance of this ADR's DESIGNED behavior is distinct from M1-D physical D3D11 validation. No implementation or merge authorized.

## Decision

For **v0.1 baseline only**, support HOST_DEVICE as the **sole advertised D3D11 device ownership mode**. SDK_DEVICE creation is deferred as a later independently negotiated capability. The host retains ownership of device, immediate context and texture/RTV; the SDK borrows them with exact call lifetime. The SDK does not assume a window or own the host's event loop.

Choose this reduced profile to avoid untestable dual-ownership lifetime/compatibility promises at first SDK release. This is a material narrowing of the original design and must update capabilities, public C ABI and release notes before contract freeze.

## Required behavior and ownership table

| Resource / action | Owner | Borrow contract | Required failure behavior |
| --- | --- | --- | --- |
| ID3D11Device | host | SDK borrows during context registration/render; device identity/generation verified | wrong-device => INVALID_ARGUMENT; device lost => DEVICE_LOST |
| ID3D11DeviceContext immediate context | host R thread | host serializes access, SDK never concurrently records on borrowed immediate context | wrong-thread => WRONG_THREAD; unavailable => BUSY |
| ID3D11RenderTargetView / texture | host | valid through rendering call and submitted GPU completion fence, unless SDK makes its own reference | invalid target/format/sample count => INVALID_ARGUMENT or UNSUPPORTED before state change |
| SDK-created vertex/index/texture buffers | SDK under host device | retired on designated R thread only after last snapshot pin and GPU completion | failed retire => BUSY, never free in-flight memory |
| Immutable CPU model / pose | SDK CPU | no GPU dependency for validated immutable CPU data | GPU device loss preserves CPU asset and logical pose |
| Offscreen output | host target | premultiplied alpha and declared sRGB/linear conversion | render failure must not corrupt previously committed logical pose |

**Host obligations:** keep device/context/RT valid for negotiated borrowing/fencing lifetime; serialize graphics R thread; signal begin_close and drain before releasing the underlying D3D11 device. SDK must not assume ownership transfer or call host callbacks while GPU/security locks held.

## State / critical section

1. Validate runtime handle generation, device ID/context/target association, expected RT format/sample count, dimensions, thread and closed state before touching D3D11 state.
2. Pin runtime GPU work, validated immutable model/pose and target resources; a failed pin returns stale/closed/busy before state mutation.
3. Capture the explicit set of pipeline states the SDK may modify (IA buffers/topology/input layout, VS/PS state, constant resources, samplers, RS viewport/scissor, OM RT/depth/blend/depth-stencil as applicable). No hidden permanent host state change.
4. Execute an isolated pass on host R thread; restore **all modified pipeline states** even after a recoverable draw error; D3D11 immediate context may not be used by a second thread until pass exit.
5. Retire deferred GPU resources using backend completion evidence (D3D11 query/fence compatible with targeted Windows feature level), then release GPU pin. Close-management drain must never release a still-in-flight resource.
6. Resize/device-loss invalidates GPU caches/generation; CPU model remains valid. Recreate GPU state explicitly after device replacement with no stale-object reuse.

No blocking external callbacks, file reads, ZIP/GLB parsing or user-code reentry while R/GPU critical section is held.

## RC1 candidate implementation-independent retirement rule (not ratified)

For baseline D3D11 feature-level compatibility, propose **D3D11_QUERY_EVENT**
with `ID3D11DeviceContext::End(query)` after SDK GPU submissions and
`GetData(query, ...)` polled ONLY by the graphics R-thread until success.
Submission completion is not implied by CPU return from `Draw` or
`Flush`. Do not busy-wait under a runtime/source/pose lock, and never
assume the event query synchronizes host presentation; presentation erasure
requires a separate host-facing fence.

SDK-owned COM references (device, context as needed, commands' input buffers,
RTV/textures if retained) and snapshot pins remain alive until the completion
witness is observed; `Release` happens on the graphics owner thread after the
observed safe point. Handle device removal/failed GetData by fail-closed
GPU quarantine or explicit safe device teardown, never by treating error as
COMPLETED. `ID3D11Fence` remains an optional separately negotiated feature,
not a minimum baseline.

Require a normative **touched-state restore list** (all IA bindings/topology,
shader and resource stages actually touched, RS viewport/scissor/state, OM
render/depth/blend bindings and state) and a reliable cleanup path even
after a recoverable GPU draw failure; disallow host immediate-context
concurrency. D3D11 resource refcounts and safe target lifetime across
in-flight draws are HOST obligations negotiated in the public descriptor;
SDK must take its own needed refs for asynchronous work.

**Design choice:** PROPOSED, subject to graphics-owner + actor-distinct
independent ratification; native GPU tests remain M1-D HOLD.

## G00 DESIGN acceptance matrix (NO GPU required)

- Source/target ownership and borrowed lifetime annotated for every public descriptor.
- Clear R-thread affinity and serial immediate-context law; reject device swap without lifecycle transition.
- State touched/restored inventory and device-loss/resize rollback design reviewed.
- Deferred retirement proof obligation names exact GPU completion signal and GPU pin ownership; release must follow completion.
- Negative GPU experiment plan includes wrong device/format/target, 100x resize/close, mid-render device loss, deferred delete after in-flight draw, host state diff and offscreen readback; if any cannot be tested on runner, evidence stays HOLD.
- Graphics owner and independent architecture reviewer record G00 ADR DESIGN decision on exact candidate SHA, **not** a GPU performance/release PASS.

## M1-D PHYSICAL validation (after design accepted)

Real Windows D3D11 hardware/software runner with host-owned test device: create/draw/readback, compare host state before/after, wrong-thread result, GPU in-flight retirement, resource leak after 100x resize/close, device-loss/reset and invalid/stale RT. Record raw logs/artifacts and tested device/driver/OS/build SHA.

Compiler smoke, contract-model and headless Linux CPU tests NEVER qualify as these native GPU tests.

## Alternative disposition

- SDK_DEVICE in v0.1: **DEFERRED**, would require separate device-creation/thread/ownership/failure matrix and non-shared host pass.
- GPU spike before G00 design acceptance: not production implementation. A separately authorized, bounded, disposable evidence-only experiment may inform a proposed ADR but cannot modify the frozen SDK contract.
- Backend-agnostic abstraction: keep as internal interface goal; do not imply a Linux/vulkan renderer exists.

## Outstanding

This ADR is not frozen until public `ars/ars.h` render/device descriptors, thread/allocator ownership, capability flags, error codes and G00 reviewer signoff agree with it. No GPU implementation evidence exists at this RC1 stage.

**VERDICT:** OWNER_PROPOSED_DESIGN; INDEPENDENT_ADR_APPROVAL_HOLD; PHYSICAL_GPU_VALIDATION_HOLD; NO_MERGE.

## Sole-owner HOST_DEVICE + D3D11_QUERY_EVENT decision (2026-10-14)

Owner selected this design, not a physical GPU PASS. After the final SDK draw on the serialized R thread, issue `End(event_query)`; `GetData` may report S_FALSE/PENDING (no release). Use bounded non-blocking progress: one initial DONOTFLUSH poll is allowed, then arrange an R-thread-only explicit progress flush if submissions would otherwise remain buffered; subsequent polls must be bounded and return BUSY to caller instead of blocking render/security locks. Report S_OK/completed only for the exact event/query and device generation. Error/removal/failed query returns DEVICE_LOST or terminal quarantine, never synthetic COMPLETED. Retain needed SDK-owned COM references and pose snapshots until physical completion; release on the R thread, with device loss or host shutdown requiring an explicit safe quarantine/release protocol. The host must not destroy borrowed immediate context/RTV before its agreed lifetime is finished. These are source-contract decisions requiring independent graphics review and actual M1-D event-query/flush, DONOTFLUSH, resize, device-loss, R-thread restore, COM reference and failure experiments; **NOT RATIFIED by actor-distinct reviewer**.
