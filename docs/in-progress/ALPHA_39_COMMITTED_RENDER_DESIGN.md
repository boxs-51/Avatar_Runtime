# Alpha #39: committed runtime pose, camera and host rendering

C1 implementation contract, 2026-10-21. User request: continue and complete
issue39. Starting source1c734a86ae43b475afc0dddaab7e57b0568aa9c8; PR75/#38
was integrated as MERGE_COMMIT83938df825d8a1618d907fc15d72904fb0d3b39f
after all11 workflows succeeded. Remote parents/tree match; local ancestry
was fast-forwarded to that merge without changing the reviewed source. No gate PASS is
claimed by this design. Frozen requirements are in build/alpha39-contract.json.

## Required observable outcome

H39-1 freezes the actual frame/pose/camera contract. H39-2 captures actual
RuntimeRouter committed snapshots and immutable configuration/model identity.
H39-3 projects native2D and GLB3D poses without mutating validated assets.
H39-4 publishes target/mode/pose/config/camera together after the exact query.
H39-5 requires independent source review and separate direct D02/D03/G03/G04/G05
assessments. H39-6 requires committed custody and honest issue/alpha disposition.
Manual scene edits or test-supplied pose counters cannot replace the producer.

The owner approved existing ABI20 render/drain in a default-OFF D3D11 profile.
Local evidence is recorded separately in the [acceptance](../completed/audits/ALPHA_39_COMMITTED_RENDER_ACCEPTANCE.md). No new public symbol,
shipping activation or default-profile capability is introduced.

## Frame and configuration contract

RenderFrameLease is caller-owned, captured once and shared as const. It retains
the actual runtime work lease, original committed SnapshotLease and immutable
ValidatedCpuModel, plus complete runtime/character/model handles, pose generation,
configuration revision and a bounded copy of local layer controls. Runtime
custody outlives every model/pose destruction and allocator callback. Its external
storage/control block cannot be allocated from the runtime Resource it pins.

Capture on R uses the same CpuOperation exclusion as H/S to copy a coherent
record and already committed pose, returning BUSY instead of waiting for an
update. It does not bind S or publish a new pose. Successful H controls affect
the next captured render configuration without changing the accepted pose
generation. Configuration revisions never wrap. A GPU candidate can contain a
requested mode/configuration; only completed host publication becomes the
accepted rendered mode. Failed candidate preparation/query/publication preserves
the previous accepted mode/frame/identity and never resets the CPU pose.

Alternative: let R read CpuContext arrays directly or edit the validated model.
Rejected because controls can tear across a frame and old in-flight frames must
retain their own immutable state. Copying the complete GLB per update is also
unnecessary: bounded node/morph overlays retain the original immutable model.

## Projection policy

Native2D starts from validated Profile B geometry. Runtime local visibility is
ANDed through the validated parent chain; opacity multiplies through that chain;
local Z ordering retains the original stable tie rules. A parameter initially
uses its validated default and maps matching committed scalar semantics. Vertex
positions use base + sum(delta_px * (parameter - parameter_default)); finite
overflow and work/memory exhaustion reject the candidate before GPU mutation.
UV/atlas/blend, part identity and triangles remain unchanged. Conflicting
semantic mappings to one parameter are explicitly unsupported, not last-wins.

GLB projection uses bounded per-node TRS and per-node morph-weight overlays.
Scalar yaw/pitch/roll are local right-handed Y/X/Z deltas from semantic neutral;
their fixed composition order is yaw * pitch * roll, composed after base rotation.
Quaternion rotation uses current * inverse(neutral) as the local rotation delta.
Translation and scale apply current-neutral offsets to the authored base TRS.
Morph semantics provide the validated absolute target weight; validated defaults
already match mesh/node weights. Defaults therefore reproduce static geometry.
An overlapping full-quaternion/Euler rotation mapping, conflicting property or
matrix-authored TRS target is typed UNSUPPORTED before GPU work. No binding is
silently dropped. Existing singular/reflected skin policies remain unchanged.

Camera world transforms are derived from that same posed scene. Model camera
selection and any host camera request must be explicit immutable inputs, never
an unrelated mutable test matrix. Projection also records target aspect/epoch.
Public camera/character selection policy must be frozen before enabling public
render; ABI20 has no separate camera setter and takes a runtime, not a character.

## Host transaction and evidence sequence

An actual runtime render host retains work from the same public CPU runtime,
creates/joins the issue38 canonical context domain, captures the actual frame,
prepares geometry/camera, then stages one owned target/query. Query completion
publishes all frame identities together; BUSY, stale generation, close, OOM,
projection or device error cannot replace the previous target. Cache/frame/query
owners and restore/readback scopes stay inside that domain. Unknown completion
retains CPU/model/pose/runtime pins through GPU surrender and opaque receipt.

Ordered implementation: CPU frame capture and lifetime negatives; neutral/dynamic
2D and GLB numeric oracles; actual host/publication bridge; optional public ABI
dispatch only after the pending profile decision; full generated-fixture WARP
and named hardware matrix. Include100 switches,100 resizes/close races,
visibility/opacity/Z, skin/morph/camera, fractionalLOD/mip pixels, source expiry/
revoke historical-pose rules, rollback, wrong-thread, stale character/model/target,
allocator reentry and exact live-object identity. Preserve all original oracles.

Local Windows evidence is primary, CI corroborates. Public/installed acceptance
cannot be inferred from native helper success. D06/D08 physical requires issue40's
disposable runner; no reset/TDR/device disable here. No private asset publication,
Cubism SDK download, main/rules/default-branch edits or product-gate promotion.

## C1: authorized public D3D11 profile

The owner selected existing C ABI render/drain in a default-OFF opt-in profile. Native-only integration cannot close issue 39. ABI20 and the public header remain unchanged. CPU-only and feature-OFF builds retain their existing behavior. Public target resources remain borrowed, with SDK custody through exact completion; GPU operations and drain stay on R. The implementation must account for every live character and define camera/composition selection before enabling capabilities. Installed static/shared consumers are required evidence.

Lifetime and publication authority are distinct. A captured frame can remain readable for retirement after character destruction or runtime close, but cannot become a newly accepted target. Final publication must serialize against close and validate complete character identity, configuration and accepted ordering. Source revocation does not retroactively invalidate an already committed CPU snapshot under the existing historical-snapshot contract; it cannot authorize a newer CPU pose.

## Public ABI20 target and composition rules

`ARS_ENABLE_PUBLIC_D3D11_HOST=ON` requires the isolated public CPU asset profile
on Windows x64. Each runtime binds R to its first render caller. All calls that
use its immediate context, including drain, are serialized on R. The host must
serialize its own context use against SDK calls and preserve valid borrowed
pointers until their nominated completion. Unsupported targets return a typed
error: this first profile supports one-mip, one-slice DEFAULT RGBA8 UNORM
Texture2D, sample count1, sRGB encoded premultiplied output, at most8192 per edge.
No public camera/placement setter exists in ABI20.

One capture census covers every live character under H/S exclusion. Characters
use stable runtime slot order. All 3D characters share world coordinates and
one camera: the first 3D character selects its lowest reachable glTF camera
node. If absent, use a fixed right-handed orthographic volume x/y[-2,2],
eye z5, near0.01/far100. Camera transforms include that frame's committed pose.
3D draws from all models share depth and are globally ordered opaque/mask then
back-to-front transparent. 2D characters then composite in slot order, each
using its own canvas-to-target scale and stable local layer/part order. No
character or required mapping is silently dropped.

Render first prepares an SDK-owned candidate target. Failed preparation or a
failed final CPU fence leaves the host target unchanged. Exact draw queries
complete before publication permission validates the full character census,
handles and configuration against close. That permission accepts the immutable
candidate at its linearization point; later H/S changes do not relabel it.
Presentation copies this accepted candidate to the borrowed host target and
uses a separate event query. Render returns BUSY while work is pending and OK
only after that copy completes. Repeated calls must nominate the same pending
RTV; a replacement RTV returns BUSY until retirement. Device identity and
generation must match the bound context. CPU pose generation does not reset on
resize or mode changes. Live drain progresses pending work once per call.

Close begins on H as before; work/snapshot custody prevents finish until R drain
completes. R drain after close retires pending work without accepting a new
candidate, releases all SDK COM owners on R, then releases the real runtime
lease. On device loss it surrenders only the SDK's registered reference graph,
uses the domain receipt, and releases historical CPU pins after that barrier;
it does not assert healthy GPU completion or teardown of external host refs.
The host must stop all further SDK use of the old context before lost-device
close/drain. Physical reset and recovery evidence remains D06/D08 HOLD. A new
runtime is required for a new device generation in this initial profile.

## Acceptance and qualification boundaries

This document defines the implemented C1 behavior; it is not an execution
receipt or gate approval. Source review and actual native evidence are recorded
separately. Completion requires actual CPU numeric, dynamic public pixels,
fractional LOD/mips, WARP/named-hardware mode/resize/close races, allocation and
injected-loss rollback, exact live-object identities, unchanged static/private
regressions and relocated C11/C++20 static/shared Release/Debug consumers.

Runtime-wide CPU/frame/model/config custody and exact accepted metadata must be
checked against the producer. Both target bytes and accepted identity must stay
unchanged on rejected candidates. Configuration and character-destroy races
must be checked while the runtime is open so close cannot mask a missing guard.

The borrowed context may contain host GS/HS/DS and predication. The SDK saves
and normalizes interfering stages and predication before uploads/draws/copies,
keeps predication disabled through query End, then restores all saved identities
and class instances. Conflicting SRV/UAV/SO bindings are rejected before SDK
mutation by the existing target preflight. A hostile host predicate test starts
from distinct destination pixels so an entirely suppressed transaction cannot
pass by leaving the previous image in place.

Actual 2D and 3D resource creation, including shader/state failures, returns
OUT_OF_BUDGET for ordinary allocation exhaustion and DEVICE_LOST for removal.
Internal native hooks reproduce allocation/query faults at the actual creation
and retirement sites; C ABI construction always supplies no hooks. Device-loss
injection does not prove physical reset or recovery. D06/D08 physical remain
HOLD under issue40, and full G01/G02/security/release qualification is separate.

No gate is closed merely because this design exists, source review passes, CI
is green or a PR is integrated. Final scoped acceptance and source/binary/log
custody must identify exactly which local execution and installed artifacts
were observed.
