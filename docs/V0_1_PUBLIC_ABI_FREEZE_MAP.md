# Avatar Runtime v0.1 — Public C ABI Completion Map (DESIGN ONLY)

**ID:** ARS-ABI-M0-01 | **Status:** BLOCKED / not frozen / not shipping  
**Inputs:** SPECIFICATION_V0.1 section 5; \`contracts/ars_p0_contract_draft.h\`; R2–R5 P0 amendments.
**Never** install the \`_p0\` proposal as \`ars/ars.h\` without the independent ABI/design gate.

## 1. What exists versus the first v0.1 public promise

| Group | Existing security draft | Missing or undecided for public v0.1 | Owner |
| --- | --- | --- | --- |
| ABI negotiation | \`ars_get_abi_version_p0\`, size/version fields, export macros | minor-extension rules, old-struct negotiation, full capabilities result, allocator/callback failure semantics | API |
| Runtime | create, begin_close, query, drain_gpu, finish_close | host device/allocator/callback descriptor completeness, runtime state transitions on lost device | core+graphics |
| Model | \`ars_model_release_p0\` | \`ars_model_load\` or create validated asset from byte stream/owned bundle, async or synchronous contract, CPU/GPU ownership | assets+API |
| Character | \`ars_character_destroy_p0\` | \`ars_character_create\`, \`ars_character_set_mode\`, visual layer controls, model-instance lifetime | character+API |
| Motion security | source_register, admin_release, rotate, revoke, submit, update | public semantic channel metadata, stable input descriptor negotiation, precise trusted-registration boundary and error taxonomy | motion+security |
| Rendering | none | \`ars_runtime_render\`, render target/device identity, HOST_DEVICE/SDK_DEVICE, source GPU owner and fence semantics | graphics |
| Observability | error codes and close counters | thread-safe error retrieval, bounded diagnostics without token/PII payloads, allocator/metric lifetime | diagnostics |
| Packaging | 14 stub-linked symbols on Windows/Linux x64 | plain-C client with **real** symbols, shared/static distribution, SDK CMake package, public header install rules | build/API |

## 2. Proposed canonical signatures to design (NOT compiled/frozen)

Preserve the R5 by-value typed handle representation. Any legacy \`ars_runtime_t*\` handle sketch is **not an alternative** to the generation-checked public ID contract.

Proposed groups, to be finalized in a complete C header:
- \`ars_get_capabilities(runtime_h, out_ars_caps_v1*)\`: struct_size negotiation, actual backend/format/quality support only; do not advertise incomplete VRM importer.
- \`ars_model_load(runtime_h, const ars_asset_source_v1*, out_model_h*)\`: host-owned memory or callback I/O with copied/borrowed lifetime made explicit; input canonicalization and semantic validator before registration or GPU upload.
- \`ars_character_create(runtime_h, model_h, const ars_character_desc_v1*, out_character_h*)\`: model referenced internally while character live; user-released model handle does not invalidate characters.
- \`ars_character_set_mode(runtime_h, character_h, ars_mode_v1)\`: prepare/rollback semantics, one host-authorized structural mutation at simulation quiescent point.
- \`ars_character_set_visual_layer(runtime_h, character_h, const ars_layer_change_v1*)\`: stable visual-layer identity, atomic mutation or explicit failure.
- \`ars_runtime_render(runtime_h, const ars_render_target_desc_v1*)\`: R-thread affinity, device/target borrowing, exact color/alpha and queue flush semantics; blocked on ADR-0001.
- Optional: \`ars_get_last_error\` with per-call or thread-local bounded lifetime; **never** a borrowed mutable global string.

These are **completion requirements**, not permission to publish ABI signatures with missing structs or unreviewed allocators. Do not implement rendering API by casting raw host pointers to a handle token.

## 3. Required ABI freeze decisions

1. Public \`ars_status\` is exactly \`int32_t\`; numeric code ownership and stable values are specified across versions. C enum types may be used for constants but not as ABI-sized return fields.
2. Public status for terminal EXPIRED, EXPLICIT_REVOKED, EXHAUSTED, clock failure/poison, QUEUE_FULL, WRONG_THREAD and BUSY has clearly differentiated caller behavior.
3. Export/call conventions compile on Windows x64 and Linux x64; actual static/shared exports, symbol names and struct sizes/offsets/alignment checked. 32-bit ABI is not implicitly supported.
4. Every input record begins with \`struct_size, abi_version\`; struct size is validated before optional field access, no reserved flag accepted nonzero; output records can negotiate caller buffer size without overwriting.
5. Every \`const char*\`/byte-span has length units, maximum, normalization and synchronous lifetime; no untrusted arbitrary pointer probes.
6. Error output handles/ingress capabilities are zeroed on any failure. Host callbacks use C linkage and have specified exception/reentry/affinity restrictions; no callback under critical security/graphics mutexes.
7. Public capability is a secret bearer granted only through trusted same-process host path; it is NOT an IPC identity and does not sandbox arbitrary third-party code in the process.
8. Distinguish user-facing logical destroy from internal CPU pin/GPU work retirement and runtime query/finish-close behavior; no UAF via stale handle.
9. Return/lease ownership of model buffers, graphics device, render-target textures, snapshots, telemetry and diagnostic strings is documented. Avoid compiler-ABI STL sharing and cross-boundary free.
10. Version availability must be testable; unsupported optional APIs MUST return \`ARS_E_UNSUPPORTED\` without advertising feature readiness.

## Audit child complete declaration candidate (NOT frozen)

Draft PR #4 adds `include/ars/ars.h` with all 21 proposed v0.1
symbols: lifecycle/close, model load/release, character create/destroy/mode/
layers, source registration/rotation/revoke, motion submit/update, capabilities,
HOST_DEVICE render and optional bounded diagnostics. `tests/abi/design_stub.c`
exports NONFUNCTIONAL UNSUPPORTED placeholders; `design_smoke.c/.cpp`
signature and layout smoke assert 16-byte typed IDs and 128-bit ingress caps.
Neither actual source behavior nor GPU/effect security is implemented.

ABI owner must ratify the optional diagnostic symbol and the exact
allocator/callback, borrowed COM target lifetime, struct_size minor-version
behavior and stable mode/capability enums. All function declarations exist for
review, but **G00 DESIGN PASS / ABI FREEZE remain HOLD** until review and CI.

## RC1 G00/G01/G10 evidence separation — owner disposition (independent review pending)

**G00 / ABI_DESIGN_FROZEN** requires a complete compilable declaration-only public C header (all model/character/capabilities/render/security/close operations), C11+C++20 signature smoke against a NONFUNCTIONAL stub, fixed-width layout and version/size negotiation *specifications*, allocator/borrow/error/thread ownership tables and independent ABI DESIGN review. Stub linking does **not** demonstrate functioning runtime. An implementer is allowed to create a header-only design candidate under M0 without claiming real M1-A.

**G01 / ABI_BUILD_IMPLEMENTED** requires an actual CMake C++20 SDK library built from source on MSVC/GCC/Clang, with exported real symbols and clean build/CTest.

**G10 / ABI_RUNTIME_CONSUMER_VALIDATED** requires an actual embedded C consumer linked to the implementation, runtime create/close/mode/render/asset and size/version/error behavior, packaging and compatibility. G10 is not a G00 prerequisite.

**No circular proof:** Remove any G00 condition that demands the actual real linked SDK. F-02/F-08 remain HOLD because the complete declaration-only surface, ownership details and ABI reviewer approval are not yet present; the 14-symbol P0 stub is partial.

## 4. Required proof before \`F-02/F-08\` may PASS

- One canonical \`include/ars/ars.h\` with no conflicting legacy pointer handle sketch.
- Compile plain C11 and C++20 consumers on MSVC/GCC/Clang and link ALL functions, including new model/render/character/capabilities operations.
- Test major/minor negotiation, old/short/long \`struct_size\`, wrong types, C layout, zero-on-failure, ABI caller/callee release and concurrency.
- Independent ABI reviewer signs a pinned candidate HEAD and publishes divergence mapping from the two original API sketches.

**Current disposition: HOLD.** The existing 14-symbol stub CI proves only limited ABI declaration/linkage, never security/runtime behavior.
