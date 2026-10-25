# Layer Editor Native viewport and Local Agent tool checkpoint

Date: 2026-10-25
Goal: AVATAR-STUDIO-LOCAL-001, Layer Editor after validated Rig selection.
Environment: real Windows Native Studio Release, one isolated private model process.
No SDK ABI, cloud, external publishing or G5 writer activation.

## Product capability introduced

- Selected layer exposes a live wireframe overlay of valid triangles and a viewport
  vertex click-target; current vertex or selected triangle receives a visible highlight.
- Existing Layers tab now has selected-layer metadata, previous/next layer, a
  wireframe switch, an isolate-view toggle, vertex previous/next, triangle previous/next,
  and selected geometry metadata. Existing Part opacity inspector is retained.
- AgentStudioTools uses the existing V1 Native IPC and single Studio owner thread:
  layer.cursor(generation) reads the selected indices and view flags;
  layer.vertex.select(generation,revision,index) picks a vertex;
  layer.triangle.select(generation,revision,index) selects a validated triangle;
  layer.overlay(generation,revision,enabled) toggles wireframe;
  layer.isolate(generation,revision,enabled) restricts rendering to the chosen layer.
- All read/interaction commands reject stale generation or Rig revision, invalid
  indices, malformed wires and invalid triangle topology. Selection state is
  private to the current model/drawable and resets on switching.
- The isolate toggle only writes a viewer rendering option, NOT mesh or model
  data. It is reversible without changing the serialized MOC3 source.
- Wireframe is bounded to 2,048 triangles, to limit UI work per frame. An
  individual valid selected triangle is still highlighted independently.

## Verification and limitations

- RED-first: the Agent operation validator rejected the five new Layer Editor
  commands; the regression case failed. Follow-up validation PASS.
- Native C11 model-free cursor test validates vertex picking, finite bounds,
  current model/drawable identity, invalid indices and triangle topology: PASS.
- Windows MSVC Native Studio Release build: PASS, existing unrelated compile
  warnings remain.
- Product physical Native Studio Agent test PASS: select vertex/triangle, stale
  index/revision/epoch rejection, wireframe/isolate roundtrip, change drawable
  and reset selection, source model mtime unchanged.
- Full Python moc3 plus model publication guard: 306 PASS, 19 SKIP, 229
  subtests PASS; selected native CTest 16/16 PASS including Layer Editor test.
- Real mouse-click placement and user visual legibility were not independently
  observed with a screenshot/GUI QA. Source wiring and Native IPC PASS are
  not a substitute for interactive acceptance.
- No direct geometry, UV, topology or layer create/delete edits are enabled.
  This is a **usable selection/inspection and view-control editor slice**,
  not a complete authoring tool.
- Existing Private Model policy and G5 writer HOLD remain unchanged.

## Next Layer Editor work, not runtime roadmap namespace

1. Verify actual mouse picking, wireframe clipping/legibility and large-mesh
   performance in an isolated Native Viewer QA.
2. Design/implement editable local source format and bounded preview drafts
   for vertex positions, UV and triangle topology with robust constraints.
   Do not mutate opaque compiled MOC3 model storage directly.
3. Add reversible commit/undo/redo, private backup/save/reopen, and Agent/UI
   parity. Validate Rig/XPBD reference consistency and accepted GPU frame.
4. Only then begin create/delete/reorder layer tooling and Model Editor authoring.

Requirements and stages belong in docs, not production code.
