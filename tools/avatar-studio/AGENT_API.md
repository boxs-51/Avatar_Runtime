# Native Studio Local Agent API (experimental)

This API is public to trusted local agents inside the enrolled Avatar_Runtime
Windows workspace. It does not extend SDK ABI20 or register new Computer Bridge
MCP tools. The interface is JSON -> validated ASCII request -> native receipt.

## Invoke

Start launch_layer_physics_studio.bat, then use:

    py scripts/studio_agent_cli.py '{"op":"studio.status"}'
    py scripts/studio_agent_cli.py '{"op":"rig.select","drawable":0}'
    py scripts/studio_agent_cli.py '{"op":"layer.topology.scan"}'

Computer Bridge can invoke the CLI with terminal_run:
workspace_alias="beta", program="python", cwd=".",
arguments=["scripts/studio_agent_cli.py", "<JSON command>"].
The enrolled default alpha workspace is not changed.

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
- Rig: rig.select, rig.vertex, rig.inspect, rig.anchor, rig.beam,
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

## Cross-drawable constraints and radial weight brush — experimental V4

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
