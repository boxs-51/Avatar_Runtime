# Alpha60 typed 2D model transfer design

2026-10-21 Asia/Bangkok. Base alpha `fa9a1a02afc8cf75c1867f58aff9f6e70fd0bdbb`.
This extends [isolated admission](ALPHA_60_ISOLATED_ADMISSION_DESIGN.md). Full issue60 remains OPEN: GLB/dual packet validation, runtime ownership, public authority and enabled installed SDK consumers remain required.

## Bounded vertical slice

A60-1b-2D transfers every field of the immutable native 2D CPU representation: canvas, RGBA atlas, flattened layers, parameters, complete semantics and mappings, parts/UV/triangles/pivots/z and deform deltas. It creates a private immutable CPU model only after independent host checks. No public registry/ABI/capability or GPU activation occurs. Valid 3D/dual inputs return UNSUPPORTED rather than silently dropping GLB. Existing native bundle parsing still validates the full bundle before that profile decision, preserving malformed-asset diagnostics.

A full combined GLB/2D/authority patch is viable but combines graph-validation and ownership trust boundaries. The selected complete 2D transfer proves the new host boundary first. GLB/dual remains a separate explicit packet profile and open acceptance item; this does not reduce the original issue scope.

## Wire version1/profile1

All integers are little endian, floats IEEE754; strings are length-prefixed strict UTF8 without NUL. No native structs, pointers, padding, encoded JSON/PNG/ZIP, helper paths or schema data cross the boundary.

Header96 bytes: magic8 `ARSCPU2D`, version u32=1, profile u32=1, total bytes u32, status u32, nonce u64, request SHA256 bytes32, payload SHA256 bytes32. Nonzero expected nonce and SHA256 of the original request bytes must match independently at the host. Payload digest checks corruption, not authentication, origin honesty or authorization. A compromised worker can recompute it, so all structural/semantic checks remain mandatory. Non-OK statuses (BAD_ASSET, OUT_OF_BUDGET, UNSUPPORTED, INTERNAL) have exactly96 bytes and SHA256(empty) payload digest, never partial data.

Success payload: canvas width/height, atlas width/height, layer/parameter/part/semantic counts, atlas byte count (nine u32). Followed in order by:

- layers: string id, i32 z, u32 visible(0/1), f32 effective opacity;
- parameters: string id, f32 min/max/initial;
- semantics: strings id/type/units/target2d, u32 flags(min/max/default bits), four f64 defaults and f64 min/max. target3d is necessarily empty in this profile;
- parts: string id, four u32 atlas rect, two f32 pivot, i32 z, u32 layer/vertex count/triangle count/deform count; vertex pairs then UV pairs, triangle triples, then each deform parameter u32 and vertex-count delta pairs;
- exactly atlas-byte-count RGBA bytes and EOF.

Counts/lengths are checked before allocation against remaining packet bytes, native schema limits and aggregate work/vertex ceilings. Strings validate UTF8/codepoint limits; semantic IDs remain canonical ASCII max128 bytes. Native parts1..128, layers1..128, parameters/semantics0..512, per-part vertices3..65535, triangles1..100000, deforms0..64. Canvas/atlas edges1..8192, total decoded pixels<=128MiB (narrowed further by packet budget), aggregate vertices<=2M and work<=8M. Layer and part IDs must satisfy their mandatory manifest binding pattern `[A-Za-z][A-Za-z0-9._-]{0,127}`; parameters and mapping targets retain128 Unicode-codepoint limits (up to512 UTF8 bytes). Mapped f64 semantic values are compared through their checked f32 projection to stored parameter values; valid non-exact decimals such as0.1 must round-trip. Layer z and part z remain native schema ranges. All floats finite; parameter/default/bounds, quaternion normalization, bool/trigger/default rules, duplicate IDs/deforms, sorted semantic IDs, mappings, rect/UV counts/triangle/layer/parameter indices are independently checked. Layer hierarchy is already flattened to effective visibility/opacity in the CPU model; the wire cannot create a hidden graph.

Total packet cap16MiB is a new explicit experiment profile under the already-used192MiB broker ceiling. The original64-byte diagnostic broker and existing MOC3 modes retain their exact caps. The dedicated new broker accepts only `--cpu-model-2d`, preserves512MiB/one process/2CPU seconds/5wall seconds and observed Job/profile cleanup. Input16MiB and worker Resource256MiB remain unchanged. Packet budget exhaustion fails transactionally; never omit model fields to fit.

## Host ownership and proof

A separate static library contains only typed packet encoding/decoding, SHA256 and PMR ownership. The native host guard links this library and scene2d only; encoded asset decoders are absent from its target/link closure. Resource must outlive every decoded model. Reader copies every value; packet destruction/mutation after decode must have no effect. Pending allocations may exist during validation but no model is published until all checks and exact EOF succeed. Failure, including OOM/BUSY, preserves the caller's existing model; temporary allocations unwind. Encoder likewise leaves an existing output unchanged on failure and requires its supplied PMR Resource for output storage. Jobs prevent reentrant mutation of the same Resource.

REQ60-T1..T5 and their regression/hostile/OOM/isolation oracles are frozen in the session contract. Generated native corpus end-to-end, canonical packet re-encode parity, prepared geometry/atlas parity, malicious digest-recomputed packets, every truncation and allocation failure, retained output/buffer lifetime and legacy probe/core/defaultOFF checks are required. Independent design and exact-diff reviews precede delivery. Local Windows results are primary; CI corroborates. No private models or physical device loss are used. Native build corrections are bounded to two per distinct error; OS failures retain proof and stop dependent runs without blind retry.
