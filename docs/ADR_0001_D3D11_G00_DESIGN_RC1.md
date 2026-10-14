# ADR-0001-RC1 — D3D11 Host-Owned Device Contract (DESIGN CANDIDATE)

**Status:** PROPOSED FOR G00 DESIGN REVIEW, NOT RATIFIED, NOT GPU-TESTED
**Decision owner:** Graphics architecture owner; independent reviewer must be distinct
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
