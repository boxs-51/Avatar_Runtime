# Avatar Runtime SDK v0.1 — Technical Specification

**ID:** ARS-SPEC-0001 | **Status:** PROPOSED / DESIGN REVIEW | **Target repo:** boxs-51/Avatar_Runtime
**Objective:** an embeddable C/C++ library to load, animate, layer and render user-authored 2D/3D characters on a host-owned update loop. This is an SDK, not a standalone avatar, camera, AI or game application.

> **R4 security contract addendum:** docs/P0_OWNER_REMEDIATION_V0.4.md supersedes R3 on irrevocable source expiry, shared per-runtime security-clock watermark and admission-vs-begin_close fence. Terminal EXPIRED and sticky CLOCK_FAULT must not be bypassed by older motion/reconnect rules. G00 remains HOLD pending independent re-audit and runtime tests.

> **M0 P0 R3 security amendment:** docs/P0_OWNER_REMEDIATION_V0.3.md supersedes R2 on rotation reservation/explicit revoke precedence, security clock authorization sampling and begin_close/publish barrier. Versioned ABI and prior lineage/sequence contracts remain governed by R2 where not overridden. These are still draft contracts; G00 HOLD.

> **M0 P0 owner amendment:** On this stacked draft branch, docs/P0_OWNER_REMEDIATION_V0.2.md and docs/ADR_P0_SECURITY_R2.md supplement/override these first-stage design sketches for handle lifetime, source authorization, retained motion provenance, atomic ingestion sequence admission, dual security/simulation clocks, fail-closed rotation, and closing semantics. These are proposed contracts pending independent review; no G00 PASS.

## 1. Normative goals and non-goals
MUST / MUST NOT denote required obligations at the indicated stage. SHOULD expresses a design preference; MAY expresses optional extension. Acceptance evidence is in docs/ACCEPTANCE_GATES_V0.1.md.

**Required:** (1) one logical Character may have native 2D and/or native 3D representations; (2) visual layers and motion layers are distinct; (3) deterministic real-time frame ingestion, retargeting and animation mixing; (4) host-owned rendering to window/offscreen target; (5) strict validation of third-party assets; (6) quality and capabilities negotiation; (7) stable, versioned C ABI; (8) isolated adapters for camera/API input and extra model formats.

**Non-goals for v0.1:** a graphical editor, native webcam capture, AI inference, a cloud service, untrusted executable plugins, proprietary Live2D integration, guaranteed 2D-to-3D reconstruction, photoreal cloth, OpenXR, a full game engine, all-platform binary support, or unconditional 60 FPS on every computer.

**Platform baseline:** Windows 10/11 x64 with D3D11 proof-of-concept. Linux x64 builds and core unit/security tests are mandatory; Linux GPU conformance is post-baseline unless an explicit backend is delivered. glTF 2.0 GLB subset + native 2D rig are first-stage formats. VRM 1.0 extension is later capability-gated. MediaPipe camera adapter is future optional integration.

## 2. Architecture and dependencies

~~~text
Host (window, network, camera, clock, allocator, scheduler)
  | C ABI + capability negotiation
  v
Runtime / CharacterInstance / ModelAsset
  |               |                      |
  v               v                      v
Motion Ingress   Asset Import/Validate   Layer/Scene Graph
  |               |                      |
Normalizer -> Retarget -> Motion Mixer -> Pose Snapshot
                                           |
                              2D Render / 3D Render
                                           |
                              Graphics Backend Interface
                                           |
                              D3D11 Target / Offscreen
~~~

| Module | Owns | Must not own |
| --- | --- | --- |
| api | opaque handles, ABI/version, errors, host callbacks | STL/exceptions across ABI |
| core | runtime lifetime, clock, character graph, diagnostics | graphics API implementations |
| assets | ARSCHAR, JSON schema, limits, importers, CPU asset cache | GPU calls, implicit internet |
| character | logical state, representation choice, rig bindings | data-source connectivity |
| layers | visible parts/order/masks; animation ownership/blend | adapter-specific parsing |
| motion | typed frames, queues, smoothing, arbitration, retargeting | OS camera APIs |
| render2d | sprite atlas, 2D deform mesh, alpha/mask passes | network logic |
| render3d | GLB mesh/skin/morph, camera, materials, transparent passes | camera tracking |
| graphics | backend, device/context/target lifecycle, fences | animation semantics |
| quality | caps, tiers, memory budgets, adaptive degrade | unapproved asset mutation |
| diagnostics | counters, tracing without sensitive payload | raw camera storage |

Dependencies point toward stable interfaces. No implicit process-wide singleton, and no arbitrary GPU context switching. The host owns the event loop, clock and update cadence. Core does not open sockets or camera devices.

## 3. Logical character and dual 2D/3D representation

**ModelAsset:** immutable validated bundle shared across instances. **CharacterInstance:** mutable pose, parameters, layers, current mode. **Representation:** one native 2D rig, native 3D rig, or an explicit 3D-to-texture projection. **RigBinding:** source-semantic-to-native-channel mapping. **RenderContext:** target/backend resources with explicit device ownership.

Modes: ARS_MODE_2D, ARS_MODE_3D, ARS_MODE_AUTO. A Character MUST have at least one valid representation. For a dual-mode character, semantic animation state belongs to CharacterInstance, not to a representation. Mode switch is PREPARE -> VALIDATE ASSET/CAPS/BUDGET -> COMMIT or ROLLBACK. Failure MUST preserve old mode and last valid pose. A 3D projection rendered in an orthographic pass is not equivalent to a native 2D rig, and MUST be advertised distinctly. No unspecified model converter may synthesize geometry.

**Internal semantic space:** right-handed, +Y up, +Z forward, meters; quaternion xyzw, radians. 2D canvas is pixels, top-left +X right/+Y down; importer/rig binding converts explicitly. GLB/VRM importers respect their original declared coordinate space and apply recorded transformations. Non-finite transforms and invalid quaternions are rejected.

Semantic parameter: ID, type (float/bool/vec2/vec3/quaternion/trigger), optional units, default/range, group, neutral fallback, mapping. Examples: head.yaw_rad, head.pitch_rad, eye.blink.left, eye.blink.right, mouth.open, expression.happy, hand.left.raise. Unknown optional channel -> ignore+diagnostic. Missing required binding -> fail validation.

### 3.1 Visual layer

VisualLayer = {id, parent, mode_scope, z_index, stable_order, visible, opacity, blend_mode, mask, slot_id}. Required v0.1 behavior: consistent stable Z order, visibility, opacity, normal alpha compositing, configurable part grouping; blend modes add/multiply and complex masks feature-gated. Cyclic hierarchy/mask edges invalid. 2D order and 3D depth sorting MUST NOT be conflated.

### 3.2 Motion layer

MotionLayer = {id, source_id, priority, weight, blend, channel_mask, bone_mask, fade_in_ms, fade_out_ms, expiry_ms}. Resolution: remove invalid/expired sources -> evaluate ownership masks -> stable descending priority with source_id tie-break -> normalized weighted override within a priority band -> bounded additive channels -> constraint clamp. Channels not authored in a representation fall back to documented neutral. An expired camera source MUST relinquish ownership so idle or API input can resume.

## 4. Bundle format and validation

Bundle extension **.arschar** is a constrained ZIP with root **character.json**, relative assets and optional digest list. Bundle is untrusted. Mandatory manifest fields: schema_version 0.1.0, bundle_id, display_name, default_mode, units, representations, rig_semantics, visual_layers and mappings; optional license/author/quality variants/digests. JSON Schema: schemas/character_manifest_v0.1.schema.json; reference: examples/character_dual_mode.json.

2D v0.1 data: PNG atlas + JSON part definitions (texture rect, pivot, relative transform, mesh vertices/weights, named deformer channels). GLB subset v0.1: embedded glTF 2.0 geometry, hierarchy, skins, morphs, PBR-base materials, basic animation; extensions are capability-listed, not silently ignored if required. VRM 1.0 is not implicitly complete merely because a GLB can be opened. A later VRM importer MUST implement its humanoid/expressions/lookAt/constraint/springBone ordering where features are advertised. Proprietary formats require separate licensed adapters.

Package validator MUST reject: absolute/UNC/drive/device paths, traversal, normalization collisions, symlinks, duplicate entries, external URI fetching, unsupported required extensions, decompression bombs, excessive entries, recursively nested payloads, nonfinite numbers, buffer overflows, mesh index errors, oversized images, invalid skinning data and embedded code/shaders.

**Provisional ceiling policy** (enforced before dangerous allocations, overridable downward): JSON <= 1 MiB, ZIP compressed <= 256 MiB, expanded <= 512 MiB, entries <= 4096, individual texture dimensions <= 8192, total vertices <= 2 million, joints <= 256, visual layers <= 128, input sources/character <=16, frames/source <=64. GPU capability and configured VRAM/RAM budgets may enforce tighter limits. These numbers are initial security ceilings, not promises that all such models run on a low-end PC.

## 5. Render API and ABI contract

Production header belongs under include/ars/ars.h. Export C calling convention with platform symbol visibility. Public structs have struct_size + version; ABI major mismatch MUST fail, minor extension is negotiated. Opaque generation-checked handles; explicit create/destroy; no exceptions, STL, compiler-dependent types, callback under mutex, cross-boundary free, or hidden global window. Application-provided callbacks/allocators must have documented lifetime and thread affinity.

Proposed C signature sketch (design only, types to be formally defined in M0 gate):

~~~c
typedef struct ars_runtime_t ars_runtime_t;
typedef struct ars_model_t ars_model_t;
typedef struct ars_character_t ars_character_t;
typedef enum {
  ARS_OK=0, ARS_E_INVALID_ARGUMENT, ARS_E_UNSUPPORTED,
  ARS_E_BAD_ASSET, ARS_E_OUT_OF_BUDGET, ARS_E_WRONG_THREAD,
  ARS_E_DEVICE_LOST, ARS_E_STALE_HANDLE, ARS_E_BUSY, ARS_E_INTERNAL
} ars_result_t;
uint32_t ars_get_abi_version(void);
ars_result_t ars_runtime_create(const ars_runtime_desc_t*, ars_runtime_t**);
ars_result_t ars_get_capabilities(ars_runtime_t*, ars_caps_t*);
ars_result_t ars_model_load(ars_runtime_t*, const ars_asset_source_t*, ars_model_t**);
ars_result_t ars_character_create(ars_runtime_t*, ars_model_t*, ars_character_t**);
ars_result_t ars_character_set_mode(ars_character_t*, ars_mode_t);
ars_result_t ars_character_set_visual_layer(ars_character_t*, const ars_layer_change_t*);
ars_result_t ars_motion_submit(ars_character_t*, const ars_motion_frame_t*);
ars_result_t ars_runtime_update(ars_runtime_t*, uint64_t now_monotonic_ns);
ars_result_t ars_runtime_render(ars_runtime_t*, const ars_render_target_t*);
ars_result_t ars_runtime_destroy(ars_runtime_t*);
~~~

The C sketch is not an implemented header and MUST NOT be represented as shipping.

Host/device ownership modes: **HOST_DEVICE** takes a borrowed D3D11 device and negotiated immediate-context access; **SDK_DEVICE** creates a private device. Explicitly validate device identity, render target dimensions/format, sample count, resource lifetime, color space, and alpha mode. v0.1 offscreen output uses premultiplied alpha with declared sRGB/linear conversion. State restoration in host mode is required: ARS restores modified D3D11 state, or host wraps ARS with a documented isolated pass verified by tests. No unguarded GPU ownership transfer or cross-device sharing. GPU resources are retired on GPU-owner thread behind fence/deferred safe point; device reset invalidates caches, not CPU model assets.

Backend interface must express caps: max texture/bone/RT format, render-to-texture, skeleton/morph support, stencil/mask, timer query, GPU memory estimates, device-loss handling. D3D11 first; bgfx adoption depends on an interop spike (backend swap is design goal, not achieved). Quality tiers LOW/BALANCED/HIGH/CUSTOM use LOD, texture caps, update frequencies, culling, batching and optional effects; absent features trigger bounded fallback or ARS_E_UNSUPPORTED. Changes use hysteresis; never degrade silently without telemetry.

Render order: update produces immutable committed pose; render snapshots that pose -> 3D opaque/depth -> 3D transparent -> 2D ordered layers -> compositing output. Pass color/depth/alpha contracts are explicit. Resize and minimized/zero-area targets are handled without use-after-free.

## 6. Motion and animation pipeline

Normalized frame: schema_version, source_id, source_generation, strictly increasing sequence, source clock domain, timestamp_ns, confidence [0..1], typed finite semantic channels, optional bones/quaternions. Inputs may originate in API client, camera adapter or manual controller. Core makes no external connection.

Ingest -> source/generation/authz validation -> bounded queue -> timestamp reordering/clock mapping -> finite/unit/range conversion -> jitter interpolation -> smoothing -> calibration -> RigBinding retarget -> priority/channel mask arbitration -> keyframe/idle blending -> constraints -> immutable PoseSnapshot -> render.

Rules:
- Ingress can be concurrent. Default queues: 64 frames/source, 16 sources/character; newest valid pose wins for high-rate continuous channels. Discrete triggers require a separately bounded event queue or an explicit reject/overflow signal (no silent loss).
- Use host-provided monotonic update clock; sources declare their clock. Adapter calibrates or arrival-times. Defaults proposed: TTL 250ms, reorder window 50ms, future skew cap 100ms; override within bounded policy.
- Drop wrong generation, duplicate or backward sequence, NaN/Inf, malformed quaternion, stale/future skewed frames. A stale source cannot retain exclusive channel ownership.
- Interpolation: scalars lerp, rotations normalized slerp; smoothing has configurable cutoff and reset on disconnect or source generation change. Never extrapolate indefinitely.
- PoseSnapshot carries simulation timestamp + monotonic pose generation; source replay MUST NOT rewind committed pose. Lost tracking fades toward idle, not frozen forever.
- Native 2D binding maps semantic parameters to deform mesh/parts; 3D maps to bone transform/morph target. Unmapped optional semantics are safe no-ops with counters.

## 7. Thread model, ownership, lifecycle

**H / control:** create/destroy, load/change structure, switch modes; serialized.
**I / ingress:** multiple threads may call ars_motion_submit, which copies validated frames into bounded queues; its only guaranteed thread-safe mutating ABI operation in v0.1.
**S / simulation:** single writer for runtime_update and pose graph; update MUST NOT overlap another update or structural mutation.
**R / GPU:** render, GPU create/upload/deletion, backend state; MUST run on device/context owning thread or negotiated context. H/S/R may be same thread; I may differ.
**A / asset workers (future):** CPU-only parse/decode; send validated immutable results to R for GPU upload.

No host callbacks while holding locks, no blocking filesystem or network work on warmed steady-state render path, no synchronous S<->R circular wait. Structural writes require explicit quiescent point. Queue size and telemetry bounded. GPU lifetime uses delayed retire after submitted work finishes.

States:
- Runtime: NEW -> INITIALIZED -> RUNNING -> QUIESCING -> DESTROYED; RUNNING -> DEVICE_LOST -> RECOVERING or QUIESCING.
- Model: UNLOADED -> VALIDATING -> READY_CPU -> READY_GPU / FAILED -> EVICTED.
- Character: CREATED -> ACTIVE / PAUSED -> DESTROY_PENDING -> DESTROYED.
- Representation swap: ACTIVE(old) -> PREPARING(new) -> COMMIT(new) or ROLLBACK(old).

Shutdown stops input, revokes source generations, drains/cancels work, suppresses callbacks, retires GPU handles on R, invalidates generation handles, frees CPU objects. Wrong-thread destruction MUST return ARS_E_WRONG_THREAD rather than freeing live GPU objects unsafely. Destroy/update and device-loss races require negative tests.

## 8. Security model

Untrusted boundaries: archive, images, GLB, 2D rig, remote frame, optional native adapter. Default deny network URI, file escape, external DLL, executable code, custom shader source. Third-party native plugins are in-process code execution and MUST NOT be treated as sandboxed: only trusted allowlisted modules in-process, otherwise out-of-process restricted worker with IPC.

Host owns camera privacy, network TLS/auth, external source registration. Runtime verifies source ID/generation/sequence and data shape; it cannot authenticate an arbitrary remote packet until adapter establishes identity. No default outbound telemetry or recording of camera frames. Structured diagnostics never include raw model payload, paths with secrets or personal image data.

Failure policy: malformed asset -> ARS_E_BAD_ASSET without partial model; resource cap -> ARS_E_OUT_OF_BUDGET preserving old scene; unsupported caps -> ARS_E_UNSUPPORTED; wrong-thread -> ARS_E_WRONG_THREAD; device-loss -> ARS_E_DEVICE_LOST; stale handle -> ARS_E_STALE_HANDLE; unexpected state -> ARS_E_INTERNAL with scoped cleanup. No speculative execution after validation failure.

Security tests: archive path/zip bomb, GLB bounds/NaN, decompression budget, fuzz parser and frame ingestion, replay/generation negative regressions, callback reentry, upload failure, detach/restart/shutdown, ASan/UBSan and dependency/license report.

## 9. Observable metrics and benchmark criteria

Emit bounded, versioned counters: load_ms, CPU/GPU memory estimate, update/render CPU p50/p95, GPU pass timing where supported, dropped stale/replay/overflow frames, queue occupancy, mode switch rollback, device reset count, remaining live handles. No claims about end-to-end camera latency from render-only tests.

Hardware profiles and reference scenes MUST be published alongside benchmark results: LOW native 2D 720p target >=30 FPS; BALANCED 3D skinned single character 1080p target >=60 FPS where supported; smoke/soak 30 minutes with no unbounded memory increase. These are intended gates pending measured reference hardware and cannot be claimed as currently attained. Quality fallback MUST return explicit reason.

## 10. Governance, release, and external references

Architecture contracts, ABI, manifest schema, thread/device ownership or trust changes are MATERIAL: update spec, JSON Schema, samples, negative tests and gates together; independent design/security review required before implementing. No automatic merge. Deliver header, CMake package, sample host, validated sample assets with redistribution rights, license/SBOM inventory, changelog and performance evidence.

Detailed gates: docs/ACCEPTANCE_GATES_V0.1.md. Design decisions/roadmap: docs/ROADMAP_AND_ADRS_V0.1.md.

External specs (inspiration, not an implementation claim):
- glTF 2.0: https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc
- VRM 1.0: https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_vrm-1.0/README.md
- bgfx: https://github.com/bkaradzic/bgfx/blob/master/docs/overview.rst
- KTX2: https://www.khronos.org/ktx/
- MediaPipe Face Landmarker: https://ai.google.dev/edge/mediapipe/solutions/vision/face_landmarker
