# PUB-01 / PUB-03 static package admission

Authorized scope: PNG and linked Physics3 admission in the isolated package worker;
independent drawable/atlas validation before GPU creation. This is a default-off
diagnostic static default-pose importer, not persistent public animation or SDK activation.
Physics is checked against the actual native parameter catalog, **validated, not simulated**.

Input is an in-memory ZIP and explicit UTF-8 manifest entry, with no extraction.
The little-endian envelope is `ARSMPK01`, u64 nonzero request nonce, u32 name length,
u32 ZIP length, name bytes, ZIP bytes, exact EOF. Limits: name 512 bytes, ZIP 96 MiB,
expanded ZIP 128 MiB, 256 entries, individual JSON 1 MiB, MOC 16 MiB,
16 atlases, edge 8192, combined decoded RGBA 128 MiB. Existing strict ZIP path,
alias, symlink, CRC, ratio and overlap checks apply. Decoding stays in the zero-capability
AppContainer with unchanged 512 MiB / 2 CPU seconds / 5 wall seconds limits.
Exceeding budgets rejects; no trusted-local fallback. ZIP byte-work has a separate
96 MiB plus 8M metadata-operation allowance (the shared ZIP primitive charges compressed
bytes as work). PNG/JSON retain a separate 8M work allowance; production limits are unchanged.

Manifest Version 3 selects Moc, Textures and optional Physics. Unknown fields in
required structures reject. Motions, Expressions, Pose, UserData and DisplayInfo
are explicitly disabled, not parsed or executed; their presence is reported as flags.
Root Groups, HitAreas and Layout are descriptive-only in this fitted static diagnostic,
not active input/hit-test/layout behavior. PNG preserves decoded RGBA8 texel values (no gamma transform). Physics3 requires
strict JSON, exact meta counts, bounded chains/vertices/bindings, valid finite ranges,
unique chains/bindings and parameter references found in the admitted MOC catalog.

Output `ARSDRAW1` is little endian, no wire structs/pointers: magic[8], u32 version=1,
u64 total bytes, u64 generation=1, u64 nonce, u32 flags, u32 atlas count,
u32 drawable count (48 bytes). Flags: bits 0/1 physics present/validated (both or neither),
bits 2..6 disabled Motions/Expressions/Pose/UserData/DisplayInfo. Each atlas is
u32 width,height,u64 RGBA length, then bytes. Each drawable is i32 texture/draw order/
render order/blend, u32 flags (visible/double-sided/inverted bits), f32 opacity,
eight f32 multiply/screen RGBA, u32 vertex/index/mask counts, then vertices
(four f32 x/y/u/v), u16 indices and u32 masks. Author identifiers are not transported;
host assigns unique synthetic indices. No shaders, filenames or handles.

Host pass one consumes only a capped 192 MiB opaque transport buffer and bounded
scratch. It checks exact framing, request/generation, flags, total bytes, atlas products,
combined counts (4096 drawables, 2M vertices, 12M indices, 2M masks), per-draw vertices
at most 65536, triangle indices, finite positions within +/-1e6, UV within +/-16,
opacity/colors [0,1], blend 0..2, all index/texture references and masks. Duplicate,
self or nested masks reject (renderer supports only direct mask geometry); mask
source index work is capped at 64 Mi entries. Array order is stable; render order ties
use source index. Only after all checks succeed may pass two allocate an owned immutable
scene. Decoder failure preserves the caller's previous scene. No D3D device, target,
resource allocation or upload may precede successful admission. Nonce binds a candidate
to one active request; cancellation/stale expected nonce rejects before GPU.

Launcher buffers bounded output and forwards it only after worker exit 0, empty Job,
and successful profile deletion; a partial pipe packet never grants readiness.
The trusted renderer accepts only the verified scene, never MOC, PNG or Physics bytes.

Acceptance: generated malformed ZIP/PNG/Physics and independent malformed wire corpus;
real AppContainer execution/cleanup; zero GPU creation for rejected input; generated
pixel oracle and private local default-pose comparison on WARP/hardware. Preserve existing
preflight, private runtime and ABI behavior. Report unrun checks and separate persistent
evaluation, SDK, sanitizer qualification and D06/D08 physical HOLDs honestly.

## Running the local proof

Build `ARS_BUILD_MOC3_PUBLIC_PIPELINE=ON` together with the existing sandbox,
asset components and (for the separate trusted host) private D3D11 options.
All defaults remain OFF. `python scripts/moc3_verify_admission_observer.py` launches
one visible static proof and a separate authenticated loopback API client. The owner-only
transient descriptor is under the ignored evidence run, removed at shutdown. `/status`
and `/frame/warp.ppm` or `/frame/hardware.ppm` are read-only observability endpoints;
there is no model upload endpoint. Tk samples and API hashes must match the same
immutable native readback. The parent captures source/dependency/binary hashes before
child creation and after exit; the child additionally checks before/after test execution.
Any drift fails closure. Assets, resized test derivatives, pixels and derived metrics
stay ignored. The suite deliberately expects an over-budget original to reject and
labels the successful resized-atlas fixture explicitly; it does not lower the production
memory oracle or automatically resize user imports.
