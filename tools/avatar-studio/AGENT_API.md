# Native Studio Local Agent API (experimental)

This API is public to trusted local agents inside the enrolled Avatar_Runtime
Windows workspace. It does not extend SDK ABI20 or register new Computer Bridge
MCP tools. The interface is JSON -> validated ASCII request -> native receipt.

## Purpose: callable tools, not Agent account management

This Local-only API exposes useful Studio work operations to Agents. It is a
**calling interface**, not an Agent-registration or per-Agent permissions
service. "Public" API naming does not authorize publication of private model
information; see [ADR-005](../../docs/reference/ADR_005_MODEL_INFORMATION_NONPUBLIC_AND_SDK_PACKAGING.md)
and [ADR-006](../../docs/reference/ADR_006_AGENT_CALLABLE_STUDIO_API.md).

The existing AgentBridge can be wrapped without a new server or identity:

    from scripts.studio_agent_api import AgentBridge
    from scripts.studio_agent_tools import AgentStudioTools
    tools = AgentStudioTools(AgentBridge(existing_studio_ipc_folder))
    catalog = tools.catalog()
    status = tools.call("studio.status")
    capabilities = tools.read_v2("avatar.capabilities")

The local IPC directory is provided by the existing Studio setup. Never
publish its path or private model outputs. The wrapper does not retry
uncertain outcomes or activate the G5 V2 writer.

## Invoke

Start launch_layer_physics_studio.bat, then use:

    py scripts/studio_agent_cli.py '{"op":"studio.status"}'
    py scripts/studio_agent_cli.py '{"op":"rig.select","drawable":0}'
    py scripts/studio_agent_cli.py '{"op":"layer.topology.scan"}'

Computer Bridge can invoke the CLI with terminal_run:
workspace_alias="beta", program="python", cwd=".",
arguments=["scripts/studio_agent_cli.py", "<JSON command>"].
The enrolled default alpha workspace is not changed.

The test-only process adapter `scripts/studio_g5_fixture_ipc.py`
routes functional calls to a synthetic Native owner through bounded
mutation and recovery file lanes. Run
`python -B -m scripts.test_studio_g5_agent_tools_ipc` after building the
opt-in Native fixture. This is not the real Studio Host transport or
a public writer SDK and must not be released as such.

## G5 draft/transaction Agent tools — synthetic Native proof, not live writer

The new `scripts/studio_g5_tools.py` offers task-oriented `G5DraftTools`
and `g5_tool_catalog()` for `draft.begin`, `draft.anchor.set`,
`draft.validate`, `draft.commit`, `draft.abort`, `draft.undo`,
`draft.redo`, `draft.status`, `draft.receipt.status` and
`draft.reconcile` (only synthetic Native, explicit recovery).
It is separately tested against the real **model-free** C11 transaction
core via the opt-in Native DLL and does not route writes to the live Studio.

See [local G5 Agent-tool test/report](../../docs/in-progress/studio-beta-20261023/G5_AGENT_TASK_TOOLS_LOCAL_T2_20261024.md).
Production V2 G5 writer remains HOLD; continue using established V1 tools
for currently supported Studio operations. Never replay a lost-ACK
mutation blindly. No Agent registration, per-Agent identity-management
system, or private model export is introduced.

## Public command groups

- Scene status: studio.status; studio.metrics.
- Playback: studio.play (enabled bool); studio.step;
  studio.expression (index 0..8).
- Scene toggles: studio.toggle (feature, enabled bool). Features:
  physics, blink, breath, gaze, speech, cycle, grid, particles.
- Motion controls: studio.setting (name, value). Names:
  motion_speed, motion_amount, wind_drive, blink_interval, cycle_interval.
- Camera and UI: studio.camera (mode fit/face), studio.zoom (value .3..3),
  studio.pan (x,y), studio.tab (index 0..3),
  studio.background (index 0..3), studio.reset.
- Local output: studio.preset (action save/load),
  studio.capture (kind viewport only).
- Parameters: param.set (index,value), param.clear (index),
  param.inspect (index), param.reset.
- Parts: part.opacity (index,value 0..1), part.inspect (index).
- Rig: rig.select, rig.vertex, rig.inspect, rig.history, rig.undo, rig.redo, rig.anchor, rig.beam,
  rig.store (action save/load), rig.apply.
- Semantic hints: layer.scan, layer.suggest, layer.adopt,
  layer.label (kind unknown/hair/skirt/strap).
- Topology: layer.topology (selected drawable), layer.topology.scan
  (aggregates of the entire model).
- Physics: xpbd.set, xpbd.read (name,value for set), xpbd.select
  (first,second,joint), xpbd.inspect, xpbd.apply, xpbd.disable;
  physics.set (legacy gravity,wind,damping).
- Input: action.trigger (kind pull/release/shake/settle, strength 0..1).

XPBD field names: gravity, wind, damping, edge_compliance, collision,
mass, contact_compliance, iterations, step, pin, substeps, pose, transfer,
bending, surface, anchors. Integer: iterations, pin, substeps, anchors.

Changing parameters does not automatically rebind XPBD: call xpbd.apply
or rig.apply separately. No semantic unknown is automatically adopted.

## Shared Rig and Layer selection

The real Native Studio owner exposes generation-fenced selection/readback
through the existing validated Local AgentBridge:

    epoch = int(tools.call("studio.epoch")["receipt"].split("=", 1)[1])
    state = tools.call("rig.selection", generation=epoch)
    # Parse bounded receipt fields: drawable, vertices, revision, saved.
    # Select only after validating the actual revision from this receipt.
    tools.call("rig.select.at", generation=epoch, revision=revision, drawable=target)
    tools.call("layer.select", generation=epoch, revision=new_revision, drawable=layer_index)
    selected_layer = tools.call("layer.inspect", generation=epoch)

Both selectors use the same live RigBank; layer.select opens the existing
Layers/Part inspector, which is **NOT** a geometry authoring tool. The
layer.inspect receipt includes selected drawable, vertex/triangle aggregates,
topology flag/islands/loose counts, and Rig revision, not private IDs/geometry.
The legacy rig.select API remains supported and uses the same owner selector.

Rig/Layer selection rejects stale model generation, stale revision,
out-of-range/unsupported drawables and malformed wire tails. The UI picker
skips drawables with zero or more than 2048 vertices; unsupported geometry
does not trap the Rig Editor navigation. Swapping drawables retains
per-layer RigBank edits and resets active undo history; an old bound
XPBD frame is not accepted as a new G5 APPLIED frame.

## Layer Editor Local toolkit (native owner thread)

The Layers tab and the local Agent share the selected drawable. View-only and
selection operations (not geometry writes) are:

- layer.cursor(generation): selected drawable, vertex, triangle, wireframe,
  isolated-view flag and current Rig revision; unset selections are -1.
- layer.vertex.select(generation,revision,index): select a validated vertex.
- layer.triangle.select(generation,revision,index): select a triangle only if
  all three vertex indices are distinct and within the current mesh bounds.
- layer.overlay(generation,revision,enabled): toggle viewport wireframe.
- layer.isolate(generation,revision,enabled): display only the active layer or
  restore all layers; the original model remains unchanged.

Read the current generation from studio.epoch and current revision from
rig.selection or layer.inspect, and refresh them after switching drawable.
Changing a layer resets its cursor; all operations reject stale generation and
revision. For full local private vertex coordinates or triangle members use
the existing G4 read-only operations with their respective epoch fences.
Do not publish model coordinates, textures, IDs or other private receipts.

The current layer editor does NOT yet modify MOC3 geometry/UV/topology or
serialize layer edits. The product G5 writer stays HOLD.

## Stable authored-layer identity (P0 contract)

Studio-authored layers have numeric ID values in 1..9999. Identity is
preserved through reorder; list index is NOT a durable reference.
Read layer.project.status(generation) to obtain current document revision:

- layer.project.by_id(generation,revision,id): resolves the current
  presentation index and metadata without changing selection.
- layer.active.id(generation,revision,id): selects that exact layer.

Stale revision or deleted ID is rejected. Reusing an ID after deletion
is a new object and needs a refreshed revision (ABA guard); numeric IDs
are not globally persistent across deletion/recreation. Historical
position-only APIs remain for compatibility and must NOT be stored as
long-lived Rig references. Other geometry mutations continue to be
index-based and revision-fenced; they require a freshly resolved index
before editing. Stable authored vertex IDs and cross-layer links are implemented
within the bounded Native Studio document. Per-vertex identity is
monotonic within its layer lifetime; vertex deletion shifts only
presentation indices, not the identity of surviving vertices.

- layer.project.point includes vertex_id.
- layer.project.vertex_by_id(generation,revision,id,vertex_id) resolves a
  stable authored vertex, rejecting stale epochs/revisions and unknown IDs.
- layer.rig.link(generation,revision,parent_id,parent_vertex_id,child_id,
  child_vertex_id,stiffness) creates a typed Rig link across two layers.
- layer.rig.unlink(...) removes that exact link.
- layer.rig.links(generation) reports count, and
  layer.rig.link.inspect(generation,revision,index) reads a bounded link.
- layer.physics.point_by_id(generation,id,vertex_id) reads a per-layer
  physical point only after GPU frame acceptance at the bound revision.

Links survive reorder, undo/redo, source reload, new-process restart and
binary load. Deleting a referenced vertex or layer is denied. Directed
link cycles are rejected. Current authored source is
STUDIO_GEOMETRY_5 (V1–V4 backward readers); runtime ARSMESH3
(reads ARSMESH1 and ARSMESH2). Neither is Cubism MOC3.

The Native Studio Links tab provides a minimal UI to mark a parent,
choose another layer/vertex as child, create, inspect pages and unlink.
Parent marking is invalidated by model/revision changes. The UI route
builds, but actual mouse-driven acceptance is GUI HOLD pending
authorized isolated-window input; it is not an independently certified
usability gate.

## Editable custom geometry layers

The locally running Studio now owns a bounded sidecar geometry document,
**separate from compiled MOC3 and RigBank**. Agent calls are through the
existing validated V1 bridge, with no network service or Agent registry:

- layer.project.status(generation): geometry document revision, layer count,
  selected authored layer and undo/redo depth.
- layer.project.layer(generation,layer), layer.project.point(generation,layer,
  vertex), and layer.project.face(generation,layer,triangle): bounded private
  readback of authored geometry and current editor revision. These receipts
  are local model data and must not be put in public logs.

- layer.create(generation,revision,id), layer.copy.drawable(generation,
  revision,drawable,id), layer.delete(generation,revision,layer),
  layer.duplicate(generation,revision,layer,id),
  layer.reorder(generation,revision,layer,target).
- layer.active(generation,layer) selects a custom layer without writing
  geometry. layer.point.add/move/delete and layer.face.add/delete edit a
  custom layer with strict epoch/revision CAS and topology validation.
- layer.edit.undo/redo(generation,revision) and
  layer.project.save/load(generation,revision). Save/load uses a fixed
  sidecar under the active local Studio evidence folder; no API path input.

Read project.status before each mutation and use its latest revision;
unknown-outcome requests must be recovered, not blindly retried.
Operations never modify the original compiled MOC3. Import captures an
eligible drawable's current geometry into a private editable snapshot.
The bounded document currently supports 8 layers, 128 vertices and 256
triangles per layer and six history snapshots. It is not an MOC3 source
writer, nor does it support custom geometry XPBD/UV/material rendering yet.

## UV, material and custom mesh physical preview

The Studio-owned editable layer document provides Local Agent calls:

- layer.uv.set(generation,revision,layer,vertex,u,v) changes a bounded per-vertex UV.
- layer.material.set(generation,revision,layer,r,g,b,opacity) edits local
  RGBA tint in the interval [0,1], including transparency.
- layer.rig.pin(generation,revision,layer,vertex) sets the local XPBD pin,
  or vertex=-1 to remove an explicit pin.
- layer.physics.bind(generation,revision,layer) binds the authored mesh
  to the native XPBD solver using its current document revision.
- layer.physics.status(generation) reports bound revision, number of
  accepted GPU frames and G5_APPLIED=0.
- layer.physics.point(generation,vertex) reads the latest physically
  deformed coordinate only after at least one GPU-accepted frame.
- layer.physics.stop(generation,revision) disables the local binding.
- layer.project.layer / layer.project.point provide private authored
  material/pin and UV readback.

Operations use the existing local IPC and sidecar revision fence, not a
public API or G5 writer. The original MOC3 remains unchanged. UV texture
display is a procedural checker preview, not source texture export.
Direct edits after binding invalidate the physical witness and require
re-binding. Native RigBank weights, interlayer joints, MOC3 material
serialization and full final image parity remain separate work.

## Live Rig history tools

The product Studio V1 API provides `rig.history(generation)`,
`rig.undo(generation,revision)`, and `rig.redo(generation,revision)`.
Use the generation read from `studio.epoch` and the latest revision from
`rig.history`. The native Studio owner compares both values before modifying
a Rig. Each successful undo/redo increments revision once and requires a fresh
`rig.apply` to update XPBD; it is **not** a G5 frame-APPLIED acknowledgment.

The current history keeps up to 16 previous edits in local memory for the
selected drawable. New edits clear redo. Switching drawable/model clears
history, so an Agent cannot accidentally undo a different layer. Studio
restart also resets history. These commands use the existing local
AgentBridge recovery/uncertain-ACK handling; never blindly retry after
a timeout. They do not enable the G5 transaction writer.

## Receipts, security and limitations

- A call succeeds only when Native Studio acknowledges it. Invalid
  commands and failed native validation return ok=false. Missing
  acknowledgment raises TimeoutError; do not infer success.
- Native parameter values must be within actual model min/max bounds.
  Native XPBD and scene controls also validate allowed ranges.
- The file-backed IPC allows one in-flight command only. There is
  no per-agent OS ACL, transaction journal, cancellation protocol or
  multi-agent serialization guarantee. Computer Bridge executes using
  inherited Windows user permissions, not an OS sandbox.
- No model path or arbitrary output path is accepted in API requests;
  no command execution, arbitrary file reads or network endpoints.
  The executable must be launched through the trusted local launcher.
- Viewport capture saves a PNG only in the ignored local Studio evidence
  folder; the API returns a success receipt, not image content.
- Preset and rig save/load validate local outcomes before success.
- Layer/topology inspection exports aggregate numeric counts only, not
  raw model identifiers, coordinates, textures or rendered images.
  A topological island is not a semantic hair/skirt identification.
- For H3 GUI interaction, use Computer Bridge's separately gated
  window-specific actions. Do not bypass desktop input denial through
  terminal access.

## Regression gates

    py -m unittest discover -s tests/moc3 -p test_studio_agent_api.py
    py scripts/test_studio_agent_api_live.py
    py scripts/test_studio_layer_scan_live.py
    py scripts/test_studio_interaction_metrics.py

Native tests: studio_rig_tests, studio_layer_analysis_tests,
studio_xpbd_parameter_tests. Test logs remain ignored under
build/studio-user.

Known missing platform features: remote/HTTP serving, a native MCP
Studio tool registered with Computer Bridge, atomic multi-command
transactions, a graphical paint-weight editor, skeletal parenting
and automatic semantic identification for all model components.

## Component-aware anchor suggestions (experimental)

Selected drawable components can now be inspected and explicitly
adopted by an agent:

    py scripts/studio_agent_cli.py '{"op":"layer.component.inspect","component":0}'
    py scripts/studio_agent_cli.py '{"op":"layer.component.adopt","component":0}'

Component numbers follow deterministic ascending first-used-vertex
order; isolated vertices are counted separately and have no automatic
anchor proposal. Each component reports vertex count, first referenced
vertex and one topmost anchor candidate. Inspect is read-only; adopt
changes only the editable rig, not the original MOC3 and not the active
XPBD solver. Apply rig separately to activate physics. The native
handler requires a rig matching the current drawable and model
signature, and rejects absent components or full anchor capacity.

This is a **geometric selection aid**, not verified skeletal rigging,
weighted bone binding or semantic hair/skirt segmentation.

## Experimental hierarchical mesh rig, local only

The native agent now supports geometric relation queries, parent-child
rig links, and per-vertex weights. A relationship query is read-only:
layer.component.relation takes two distinct component IDs of the
selected drawable and returns a nearest vertex pair and gap. Nearby
components are not automatically connected or considered related
body parts.

In the Rig Editor, use the Hierarchy toggle to switch to advanced
editing. Mark a parent vertex, select another vertex and Attach child.
Cycles, self-parenting, and a second parent per child are rejected.
Remove child's parent unlinks the child. The weight slider controls
the selected vertex between 0 (fixed at source) and 1 (full
simulation). Apply rig is required to rebind XPBD after any edit.

Local Agent JSON command examples:
    {"op":"rig.parent","parent":0,"child":2,"stiffness":0.85,"enabled":true}
    {"op":"rig.parent.inspect","child":2}
    {"op":"rig.weight","vertex":3,"weight":0.25}
    {"op":"rig.weight.inspect","vertex":3}
    {"op":"layer.component.relation","first":0,"second":1}
    {"op":"rig.store","action":"save"}
    {"op":"rig.apply"}

V3 rig files (STUDIO_RIG_3) persist parent edges and sparse vertex
weights, with backward reads for V1 and V2. The RigBank stores edited
rigs only: switching among hundreds of untouched drawables is safe,
but the cache still has a 32-edited-rig limit.

Current hierarchy is an XPBD parent-distance constraint, not a
complete hierarchical skeletal transform/skin-weight binding.
Explicit cross-drawable parent links and automatic semantic body
part identification remain out of scope for this version. Geometry
does not imply anatomy. Model assets, textures and screenshots remain
outside Git.

Tests:
    build/avatar-studio/Release/studio_rig_tests.exe
    build/avatar-studio/Release/studio_layer_analysis_tests.exe
    build/avatar-studio/Release/studio_xpbd_parameter_tests.exe
    py scripts/test_studio_hierarchy_live.py

The Beta Auditor session marker describes a different review-only
session, with pending findings and separate temporary IPC. This
implementation does not modify the auditor marker or IPC and does
not claim independent auditor approval.

## Cross-drawable constraints and radial weight brush - experimental V4

Native Rig Editor has three panels: Basic, Hierarchy and Cross.
Hierarchy lets a user pick a vertex, select Brush radius and Target
weight, then paint a radial region with smoothstep falloff. The source
geometry is never edited. Painting affects only the editable rig and
requires a separate Apply rig command for XPBD. Invalid input or a full
256-entry sparse weight table leaves the previous rig unchanged.

The Cross panel selects a second drawable and a child vertex, and
offers an explicit Attach to selected parent action. Geometric
proximity, layer names or rendering order do not authorize automatic
attachment. XPBD currently supports at most TWO drawables with at most
2048 vertices combined. Up to 16 links can target ONE other drawable
in the current source rig; a different target is rejected. Links are
distance constraints, not skeletal transform parenting.

Agent API additions:
- rig.cross: target drawable, parent source vertex, child target
  vertex, stiffness 0..1, enabled boolean.
- rig.cross.inspect: returns numeric link count and target.
- rig.brush: radius >0 and <=1000 model-coordinate units, weight 0..1;
  centered on the selected rig.vertex.
- rig.inspect: now includes cross-link count.

Local STUDIO_RIG_4 presets save cross constraints, parent links
and sparse weights. Readers still accept V1, V2 and V3. A corrupted
or conflicting link file is rejected without mutating the rig.
Target vertex counts are checked against the actual model on apply.
Sparse RigBank retains edited cross links across drawable navigation.

Regression:
  py scripts/test_studio_cross_brush_live.py
  build/avatar-studio/Release/studio_rig_tests.exe
  build/avatar-studio/Release/studio_xpbd_parameter_tests.exe

No private models, textures, names, screenshots or raw vertex
coordinates are exported by these commands. Computer Bridge still
uses inherited Windows user permissions and the local file IPC is
not an independently authenticated security boundary. The separate
Beta Auditor session remains read-only; its findings are pending.

## 2026-10-23 remediation evidence gate

Only `safe_case` / `safe_json` from `scripts/studio_public_report.py` may encode a public-safe categorical Studio test result for this workflow. Unknown keys, raw receipts, per-model numeric measurements, paths and images fail validation. This is an opt-in reporter primitive; until every artifact promotion path enforces it, finding F04 remains PARTIAL.

Native Agent IPC now uses bounded wire input and strict trailing-token checks. Python cooperating writers use a cross-process Windows file lock; the lock file is not an ownership marker and remains on disk after release. An orphaned request must be quarantined and examined with the Native session stopped before manual cleanup. This is not an OS access-control sandbox or a proven exactly-once queue; finding F02 remains PARTIAL.

## Agent Toolkit V2 discovery prototype (ADR-002, local only)

V2 is **not** a Native Studio API or Computer Bridge Avatar plugin yet.
It is an isolated, non-mutating discovery interface with two supported
operations and no direct model or filesystem access:

    py scripts/avatar_toolkit_v2_cli.py capabilities
    py scripts/avatar_toolkit_v2_cli.py schema avatar.capabilities
    py scripts/avatar_toolkit_v2_cli.py --stdin

The stdin form expects a complete V2 JSON envelope and always returns a
bounded JSON response. Only avatar.capabilities and avatar.schema may
return COMPLETED_READ. Session ID, model generation and edit revision
must currently be null; mode must be read. Editing, private inspection,
session.open, real Native V2 and registration as an H5 Avatar tool-group
remain disabled by design, pending ADR-002 G2/G3.

The decoder validates at most 65536 bytes, exact envelope fields,
UTF-8, duplicate-key rejection, finite numbers, request ID and
operation schemas. Do not interpret discovery success as Native session
authorization. Beta release gate remains HOLD.

## Local-only Studio reads (ADR-002 G4 partial, 2026-10-23)

The updated Local-only ADR-002 supersedes mandatory named pipes,
distributed journaling and Computer Bridge tool registration.
The V2 library can access existing Native V1 read commands ONLY when
a trusted local caller injects an already-created AgentBridge and sets
allow_local_private=True. No arbitrary model or IPC path is accepted
inside V2 JSON. Offline CLI remains discovery-only.

Supported conditional read calls:
- studio.local.status: frame/playing/XPBD flags and aggregate counts.
- catalog.parameters.page: start index 0..2047, limit 1..4;
  returns bounded index, numeric value/range and pin state.
- catalog.parts.page: same pagination; returns index and opacity.

These operations return classification LOCAL_PRIVATE, backend
legacy-v1-read-only and generation_available=false. Page reads
check aggregate counts before and after the page and fail if changed,
but are NOT atomic snapshots: a same-count model reload can remain
invisible until a Native model-generation fence is implemented.
Do not publish local geometry, model IDs or numeric model observations
in public reports; keep all private readback in ignored local storage.

Python in-process use:
    from scripts.avatar_toolkit_v2 import execute_request
    # local_bridge is an existing AgentBridge for an Owner-launched Studio.
    response = execute_request(request_json_bytes,
        local_bridge=local_bridge, allow_local_private=True)

Local readback is NOT an authentication layer against same-user
processes. Mutating V2 opcodes, Native V2 dispatch and model lifecycle
are not implemented. G4 and release remain HOLD/PARTIAL.

## Update: Native G4 model-generation fence (2026-10-23)

The earlier Local-only V1 adapter section described best-effort
count-only paging. That description is superseded for the current
Native Studio Release and RelWithDebInfo binaries.

Native STUDIO_EPOCH supplies an opaque, locally generated model
generation. PARAM_INSPECT_AT and PART_INSPECT_AT accept a
positive generation and index, rejecting mismatched loads on the
model-owning thread. V2 local read results now report
generation_available=true, model_generation (LOCAL_PRIVATE),
and editor_revision_available=false. Bounded pages use guarded
Native item reads and post-read generation verification.

V2 LOCAL_PRIVATE request envelopes may set expected_model_generation
to the generation returned by an earlier authorized local read.
Mismatched generations yield REJECTED/STALE_MODEL. This field
remains unsupported on public/offline discovery, and it does NOT
enable expected_edit_revision, mutating commands or session leases.
A page is model-generation-safe but not an atomic animation-frame
snapshot; editor-revision CAS and accepted XPBD frame fences
are still pending. Studio RELEASE HOLD remains in effect.

## F02 Native ACK publication update (2026-10-23)

An existing Native ACK file was previously visible while incomplete.
The latest Native Studio creates a temporary response and atomically
publishes it only after checking write, flush and close. The
complete-frame V1 client oracle is retained. Source and test evidence
are described in F02_NATIVE_REPLY_ATOMIC_20261023.md. F02 remains
PARTIAL because direct-writer ownership, crash/restart and ACK-loss
semantic tests are not complete. Public SDK ABI20 is unchanged.

## G4 local model ID catalog and lookup (2026-10-23)

Trusted-local V2 readback now supports catalog.parameter.lookup and
catalog.part.lookup with arguments containing exactly one model ID.
Bounded catalog.parameters.page/catalog.parts.page results include
an ID string with each numeric record. Native read opcodes:
PARAM_ID_AT, PART_ID_AT, PARAM_FIND_AT and PART_FIND_AT. All verify
the model generation on the Native owner thread; FIND_AT returns
an index only for a unique exact match. Duplicates and unknown IDs
fail closed.

Local-only ID acceptance for this initial slice: ASCII letters,
digits, underscore, hyphen, dot and colon, length 1..59 bytes.
Other IDs fail closed rather than being truncated or serialized
unsafely. The return data is LOCAL_PRIVATE and must not be placed
in public reports/CI/Git. A successful lookup is not an edit
authorization, frame acceptance or an editor revision fence.

The default CLI stays discovery-only without an explicitly supplied
trusted-local bridge and private read opt-in. G4 and RELEASE remain
partial/HOLD.

## F02 - ACK owner fence and retry (2026-10-23)

Native Studio retains a completed response file across temporary
Win32 replacement denial and attempts publication on later frames.
It refuses to admit a new request while a prior response exists.
The command is never re-applied to retry publishing an ACK.
This guards ordinary local-client ownership ordering; it does NOT
provide authentication or crash-durable exactly-once semantics.
See F02_ACK_OWNERSHIP_AND_DEFERRED_PUBLISH_20261023.md.
## F02 client-side uncertain-outcome admission (2026-10-23)

The Python AgentBridge now creates a local uncertain_outcome marker
with a random request token, flushes and fsyncs it before publishing
request.txt. A full own-token typed ACK permits cleanup; on timeout
or incomplete ACK the marker is kept and subsequent cooperating
calls fail closed rather than silently retry a potentially applied
mutation. A stale orphan response.tmp also blocks submission.

Do not delete this marker or repeat non-idempotent write operations
without independent Native state reconciliation. It is not a Native
commit journal and does not protect IPC files against processes
running with the same Windows identity. Native accepted frames and
editor revisions still have separate gates; Studio release HOLD.
## UTF-8 model ID transport update (G4, 2026-10-23)

The historical ASCII 1..59 ID limit is superseded for the current
development branch. Local parameter and part read operations now
transport ID bytes using lowercase hexadecimal tokens. Native
PARAM_ID_AT and PART_ID_AT reply with index and idhex; Native
PARAM_FIND_AT and PART_FIND_AT accept a hex UTF-8 ID. The trusted
local V2 adapter accepts a decoded Unicode ID and serializes it
to hex via the V1 AgentBridge.

Identifier constraints: UTF-8 1..96 bytes; slash, backslash,
C0/C1 controls, NUL, invalid UTF-8 and overlong bytes are rejected.
No normalization is performed; lookups are exact. Unknown and
duplicate matches fail closed. The result classification is
LOCAL_PRIVATE; do not publish IDs or model geometry. Existing
generation fences and private-access guards remain mandatory.

Unicode unit fixtures passed, as did Native Release grammar and
current physical model E2E. A true Unicode-ID MOC3 physical fixture
is NOT_RUN, so G4 remains PARTIAL and product release stays HOLD.


## Real Studio G5 readiness (read-only, writer HOLD)

The existing local V1 Studio bridge now supports `studio.g5.status`.
Agent code may call `AgentStudioTools(existing_bridge).g5_status()` to
inspect `g5_writer_enabled=False`, `g5_transaction_connected=False`,
`frame_acceptance_verified=False`, model generation and legacy Rig revision.
The shipped Studio returns only bounded LOCAL_PRIVATE metadata. Legacy Rig
revision is **not** the G5 CAS document revision, and this feature **does not
enable** G5 writing or certify solver/GPU frame acceptance. See
[real Studio read-only checkpoint](../../docs/in-progress/studio-beta-20261023/G5_REAL_STUDIO_READONLY_STATUS_20261024.md).

## Real Studio UI/V1 Rig dispatch and recovery — G5 stage

The real Studio now routes UI and V1/Agent Rig edit operations through
a shared private C11 owner-thread candidate/commit dispatcher with
generation/revision validation. This preserves legacy Rig edit semantics
and does NOT enable the experimental G5Bus draft writer or claim a frame
was applied to XPBD/GPU.

The production local AgentBridge offers `recover(original_token)` and
`reconcile(original_token)`. A command timeout raises
`StudioUnknownOutcome` carrying the original local request token.
The Studio's separate `api/recovery` lane returns only the original
OK/ERR receipt metadata or UNKNOWN. Reconciliation can clear the
local uncertain-outcome marker only after a matching FOUND receipt;
UNKNOWN never causes retry or automatic clearance.

The Agent's callable helpers are `AgentStudioTools.recover_v1` and
`AgentStudioTools.reconcile_v1`. No Agent accounts, registration or public
network permissions are introduced. See [implementation and gate
report](../../docs/in-progress/studio-beta-20261023/G5_LIVE_RIG_DISPATCH_V1_RECOVERY_20261024.md).


## Local legacy Rig frame witness (not G5 APPLIED)

The existing local V1 bridge supports
`studio.g5.legacy_frame.status` and the Agent helper
`AgentStudioTools(existing_bridge).legacy_frame_status()`.
It reports only bounded LOCAL_PRIVATE model-generation, legacy revision,
Rig bind, frame-accept and currentness metadata. The Studio records this
after a successful XPBD preparation AND GL/GPU render acceptance. Any Rig
edit makes the old accepted revision stale until a new bind/render succeeds.

The result explicitly sets `g5_writer_enabled=False` and
`g5_applied=False`. Do not report an accepted legacy Rig frame as a G5
transaction APPLIED. The actual G5Bus document-to-frame gate is still HOLD.
See [G5 T3 local checkpoint](../../docs/in-progress/studio-beta-20261023/G5_T3_NATIVE_CANDIDATE_FRAME_WITNESS_20261024.md).
