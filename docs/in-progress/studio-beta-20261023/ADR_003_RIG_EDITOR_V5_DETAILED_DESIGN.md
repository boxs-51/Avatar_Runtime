# ADR-003 / DES-V5-001 — Avatar Studio Rig Editor V5 (detailed design)

- Version: 0.2 / 2026-10-23 (Local-only scope reconciliation)
- Status: PROPOSED / DESIGN-ONLY / LOCAL-ONLY / RELEASE HOLD
- Author role: Architecture & research handoff (self-review, not independent approval)
- Owner/approval: Avatar_Runtime repository Owner, decision pending
- Workspace: Computer Bridge alias beta, branch exp/layer-physics-native-studio-local
- Inspected HEAD: 05df4d374414a6062e69f766eb0b42f25be885b8 (dirty working tree; not a source-freeze claim)
- Parent decisions: ADR-001 hierarchical skinning (PROPOSED); ADR-002 Avatar Toolkit/API v2 (PROPOSED)
- Constraints: no public ARS ABI20 changes; preserve V1–V4 files and existing XPBD; V5 OFF by default; no source patches/commit/merge/deploy
- Evidence policy: PUBLIC-SAFE SYNTHETIC DESIGN; never export private model identifiers, coordinates, images, derived model hashes, object IDs, or raw native receipts.

## 0. Scope, evidence and decision

C0: Specify an implementable, testable 2D bone/skin editor with per-vertex
multi-influence weights, cross-drawable transforms, reversible edits, a
generation-safe XPBD/render pipeline and a practical human/Agent UI.

OBSERVED (source-grounded):
- tools/avatar-studio/studio_rig.h: V4 per-drawable rig (64 anchors,
  128 beams, 64 vertex parent-distance edges, 256 sparse scalar
  physics weights, 16 cross constraints, bank of 32 edited rigs).
  V4 "parent" is NOT a transform bone; V4 weight modulates physics
  participation and is NOT a per-bone skin influence.
- tools/avatar-studio/studio_xpbd.cpp: binds one/two selected drawables,
  at most 2048 vertices total, extracts Core drawable positions,
  stages solver and display-filter candidates, substitutes render
  pointers and accepts solver state after GPU render check.
- tools/avatar-studio/native_mesh_xpbd.hpp: dynamic nodes follow an
  input source pose, solve links/pose constraints, validate finite
  state and increment candidate revision on accept.
- tools/avatar-studio/studio.c: update() drives Core parameters,
  linked Physics3 and csmUpdateModel before XPBD preparation; one
  frame() owns UI, model update, drawing and agent request polling.
  Rig editor UI is one right-side panel with Basic/Hierarchy/Cross modes.
  Current hierarchy is edge constraints, not skeletal transforms.
- tools/avatar-studio/AGENT_API.md and scripts/studio_agent_api.py:
  experimental Studio Agent v1 JSON -> ASCII file IPC; no v2 tools.
  Current uncommitted work contains candidate security/IPC/durable
  save improvements. Six Python API unit tests PASS but do not
  demonstrate full Windows physical or privacy clearance.
- docs/BETA_EXPANDED_QUALITY_SECURITY_ARCH_AUDIT_20261023.md:
  historical F01 parser (P1), F02 IPC (P2), F03/F04 privacy (P2),
  F05 persistence (P2), F06/F07 coupling and coverage.
  F01–F05 are NOT declared closed by this design.

PROPOSED: implement V5 as an opt-in editor module and a sidecar V5
RigDocument, with a typed v2 API, not as reinterpretation of V4 fields.
The Studio model, animation, physics and rendered frame remain
single-owner on the UI/render thread.

Requirements (traceable below):
- REQ-V5-01: true acyclic 2D parent/child bones and stable IDs.
- REQ-V5-02: at most four deterministic skin influences per vertex.
- REQ-V5-03: cross-drawable links in a single well-defined model canvas.
- REQ-V5-04: immutable source geometry and pose/solver separation.
- REQ-V5-05: undo/redo with atomic edit/commit and revision CAS.
- REQ-V5-06: no stale frame/solver commit after model, rig or session change.
- REQ-V5-07: resilient V5 persistence and V1–V4 preservation.
- REQ-V5-08: discoverable, accessible GUI and matching Agent tools.
- REQ-V5-09: private-safe diagnostics, bounded input and deny-by-default.
- REQ-V5-10: reproducible RED-first synthetic, Windows physical and
  performance gates before enablement.

## 1. Architecture and module boundaries

Conceptual component diagram (NOT current source layout):

    Human Rig UI                   Local Agent API v2
          |                              |
          +----------- CommandBus <------+
                        |
                  Schema/Capability
                        |
         SessionEpoch / ModelGeneration / EditorRevision
                        |
                EditorDocumentStore
           /            |               \
    BoneGraph      SkinWeightStore    LegacyV4Constraints
           \            |               /
                  DraftValidator
                        |
         DraftCommit / UndoJournal / Persistence
                        |
            FrameCoordinator (single owner)
              |                 |
      Animated Core pose     V5 rig snapshot
              \                 /
               SkinningEvaluator
                       |
            ScenePoseCandidate (2D)
                       |
             XPBDAdapter -> SolverCandidate
                       |
             RenderCandidate / GPU gate
                       |
              AcceptedFrame state
                       |
          Renderer / GUI overlay / diagnostics

Recommended internal modules:
- rig_v5_document: typed data, canonical normalization, identity,
  migration metadata and private topology binding; no renderer access.
- rig_v5_bones: graph validation, topological order, local/bind/world
  affine 2D transforms and pose evaluation.
- rig_v5_skin: packed four-slot influence table, paint kernels,
  normalization and additive deformation computation.
- rig_v5_spaces: drawable-local -> rig canvas -> viewport conversions.
- rig_v5_transactions: copy-on-write drafts, CAS, journal, undo/redo.
- rig_v5_runtime_bridge: converts immutable mesh snapshot and skin pose
  to XPBD input; refuses unsupported size/topology.
- rig_v5_editor_ui: tree, panels, overlay, hit-testing; emits commands.
- rig_v5_agent_adapter: small local command handler using the same editor
  validation path as the UI; no new service, dynamic plugin manager or
  unguarded native mutation.

Ownership invariants:
1. Core owns immutable decoded assets and its own current animated mesh;
   V5 NEVER writes into source asset memory or persists derived geometry.
2. RigDocument owns authored bones, bind pose, influence weights and
   legacy constraints; SkeletonPose owns transient evaluated matrices.
3. XPBD owns accepted dynamic state. FrameCoordinator alone commits
   the accepted presentation snapshot and generation.
4. Viewport selection is an editor-only handle. Agent mutations name
   explicit target, document revision and model generation.
5. Every cross-drawable reference is resolved in a validated canonical
   canvas space before any solver binding or rendering.

## 2. V5 rig document: strict data contract

Recommended on-disk V5: separate versioned sidecar document, canonical
UTF-8 JSON plus version/length checks in a bounded reader; native
binary/CBOR could be a future optimization after a schema is frozen.
No externally supplied absolute path; storage root is Owner-enrolled
local ignored Studio project store. No arbitrary file input/output
through Agent API. Reject duplicate JSON keys, NaN/Inf, unknown
required fields, excess recursion, overlong strings and invalid
Unicode. Reject invalid entire document, not partial apply.

Proposed top-level schema (synthetic identifiers):

    {
      "schema": "avatar.studio.rig",
      "major": 5,
      "minor": 0,
      "document_id": "rig-synthetic",
      "model_binding": {
        "model_generation_at_edit": 7,
        "topology_binding": "private-opaque-internal",
        "drawable_vertex_counts": [4, 4]
      },
      "coordinate_convention": "rig_canvas_xy_column_vector_v1",
      "skeletons": [{
        "id": "skeleton-0",
        "root_canvas": "model",
        "bones": [{
          "id": "bone-root",
          "parent_id": null,
          "owner_drawable": 0,
          "bind_local": {"tx": 0, "ty": 0, "rotation_rad": 0,
                         "scale_x": 1, "scale_y": 1},
          "pivot_local": [0, 0]
        }]
      }],
      "skin": [{
        "drawable": 0,
        "vertex": 0,
        "influences": [{"bone_id": "bone-root", "weight": 1.0}]
      }],
      "legacy_v4": {"preserved": true},
      "editor_meta": {"last_saved_revision": 0}
    }

This is an illustrative shape, NOT a deployed parser or permission to
serialize real model data to public docs. Actual private topology binding
may use a canonical fingerprint calculated and held in the private local
store. Existing V4 signature() is based on parameter and part IDs, not
a complete topology identity; do not treat it alone as a sufficient
collision-resistant V5 topology witness.

ID/identity:
- document_id: locally generated opaque 128-bit identifier, stable across
  document edits and unchanged across undo.
- bone_id: stable opaque 128-bit ID; not based on bone name, array index,
  texture path or source model identifiers. Names are optional LOCAL
  editor presentation, PRIVATE on export.
- vertex handle: (drawable stable binding, zero-based vertex index,
  model topology binding). Index is meaningful ONLY for matching
  model_generation/topology, not across arbitrary reloads.
- skeleton ID: stable within document; no implicit relationship to
  draw order or layer semantic labels.
- canonical order: skeleton ID, parent-before-child topological order
  with deterministic ID tie-break; influence list by stable bone ID.
- document_revision: unsigned 64-bit monotonic session revision; every
  successful edit/undo/redo creates a NEW revision. Revision is never
  decremented for undo. New session gets a distinct session epoch.
- draft_revision: private staging branch; only CAS commit yields new
  document_revision.

Proposed initial beta budgets, NOT guarantees:
- 64 bones per skeleton, 128 bones per document, 4 bone influences/vertex.
- For a V5 + XPBD combined session retain CURRENT native limit:
  <=2 selected drawables, <=2048 total selected vertices. Do not
  advertise broader solver support because a skin evaluator can
  process more geometry.
- 256 draft history commands, <=8 MiB retained diff/snapshots per
  session; fail OUT_OF_BUDGET rather than silently dropping live state.
- Agent payload obeys ADR-002 tentative bound <=64 KiB request.
  Large private influence data requires paged, validated sessions,
  not an unbounded JSON blob. All limits discoverable via capabilities.

Strict predicates: no orphan bone, duplicate ID, self-parent, bone
cycle, invalid cross-reference, out-of-range vertex, duplicate weight
slot, degenerate bind matrix or unbounded geometry. A V5 load is
fully checked into a fresh candidate before replacing the active doc.

## 3. Bone DAG and 2D affine convention (REQ-01)

Coordinate convention: 2D homogeneous column vectors (x, y, 1)^T.
Use 3x3 affine matrices; multiplication A*B means B acts first.
RigCanvas is in MODEL units and Y-axis orientation must be read from
the active Core/view transform. Do not derive anatomy from +Y or from
a layer name. Angle units are radians, positive CCW in the explicit
RigCanvas basis. Screen Y inversion belongs only to viewport mapping.

For each bone b:
- BindLocal_b = Translate(bind_origin_x, bind_origin_y)
                * Rotate(bind_angle) * Scale(bind_sx, bind_sy).
- BindWorld_b = BindWorld_parent * BindLocal_b
  (root's parent is the skeleton's RigCanvas root transform).
- PoseDelta_b = Translate(delta_x, delta_y) *
                Rotate(delta_angle) *
                Scale(delta_sx, delta_sy).
- PoseWorld_b = PoseWorld_parent * BindLocal_b * PoseDelta_b.
- Deform_b = PoseWorld_b * inverse(BindWorld_b).

Pivot is specified in bone-local space, not as a screen pixel:
editing a pivot recomputes bind_local and inverse bind using a
freeze-preserve-world operation if chosen by the UI. The V5 beta
default uses an origin-at-joint bone transform, with pivot_local
informational/authoring until its freeze semantics are proved.
Never silently alter existing skin deformation when user changes
the pivot. Define explicit Rebind / Preserve visual pose actions.

Validation:
- A DAG has one or more roots, each child at most one transform
  parent; any parent ID must exist in the same skeleton group.
- Deterministic Kahn topological sort; cycle/repeated ID rejected
  with no document delta. Graph changes validate all descendants.
- Finite float64 calculation; determinant magnitude >=1e-8
  (provisional, dimensionless for normalized affine submatrix).
  Beta editing forbids zero/negative scales, unbounded translations,
  and implicit mirror. Reflected source geometry may be represented
  by a known validated drawable-to-canvas transform, not hidden
  in per-bone scale. Extend only with tests for winding reversal.
- Normalize rotation representation to (-pi, pi] upon serialization;
  round-trip tolerance must be frozen in tests before implementation.
- Parent changes have explicit preserve_world=true/false behavior.
  With preserve_world: new local bind = inverse(new parent's bind
  world) * old bind world; a candidate must be decomposable to
  permitted TRS with no hidden shear, otherwise reject.
- Reparenting an animated bone is a document edit and must rebind/
  invalidate previews and solver caches atomically.

Bone constraints and V4 XPBD "parents" are different namespaces:
  bone.parent   = parent transform graph edge
  rig.constraint.upsert(kind="parent_distance")
                = solver edge between vertices
Neither is a semantic alias for the other.

## 4. Skin weights / painting / blending (REQ-02, REQ-04)

Each vertex has either an empty list (identity/no added bone skin)
or 1..4 unique influences summing to 1 within tolerance 1e-5
(provisional). Each weight finite in [0,1]; zero slots eliminated.
Sort by bone ID for deterministic storage. If a paint operation
would create a fifth influence it must reject or use an explicit
user-selected "replace weakest unlocked influence" mode; never
silently discard a bone. Lock flags on bones/vertices are enforced.

At bind time store the approved neutral bind mesh positions B_d
privately, with the exact model/topology/space generation. Core
produces the animated source positions A_d(t) every frame.

For a vertex v in drawable d:
- b = C_d * B_d[v] (bind position in RigCanvas)
- a = C_d * A_d(t)[v] (current source animated position)
- D_i(t) = PoseWorld_i(t) * inverse(BindWorld_i)
- delta_v(t) = sum_i w_vi * ( D_i(t) * b - b )
- target_canvas_v(t) = a + delta_v(t)
- target_drawable_v(t) = inverse(C_d) * target_canvas_v(t)

Here C_d maps drawable-local Core positions into RigCanvas. Empty
weight list yields target = original animated Core position.
Identity bone transforms yield exactly source animation (within
numerical tolerance). This ADDITIVE deformation policy prevents
applying the same Core deformation twice; it differs deliberately
from naively skinning A_d(t) through D_i on top of Core animation.
The additive policy has known limitations when authored bones should
co-rotate with extreme nonlinear MOC3 deformations; test/reference
those cases separately before offering alternative skin modes.

Brush contract:
- tools: Add, Replace, Subtract, Smooth, Normalize, Flood Connected,
  Mirror (Mirror DISABLED until correspondence proof).
- explicit drawable, bone, center in neutral RigCanvas or stable
  vertex handle, radius in RigCanvas model units, falloff curve,
  strength, symmetry setting and locked influences/vertices.
- proposal for radial strength q in [0,1]:
    distance = |b_vertex - b_center| / max(radius, epsilon)
    falloff = smoothstep(1 - clamp(distance, 0, 1))
    proposed_weight = lerp(old_weight, target_weight,
                            q * falloff)
- for Add/Replace/Subtract: preserve locked influences, set selected
  weight subject to remaining mass, scale only unlocked other
  influences by remaining mass; if no remainder and no valid slot,
  reject or explicitly remove the influence.
- for Smooth: neighbors are derived from validated triangle topology
  and normalized, never from raw screen proximity across disconnected
  mesh islands. Do not diffuse over disconnected components.
- Preview kernel uses immutable bind geometry and returns only
  private draft metrics. The complete stroke (pointer down -> up)
  is ONE undoable edit, even if preview has dozens of samples.
- Paint does not live-write solver state. Draft needs validation,
  apply/rebind and frame acceptance before tagged Applied status.

XPBD physics scalar weight from V4 is separate from V5 skin weights.
Naming in UI must clearly distinguish "Bone influence (%)" from
"Physics mobility (0 fixed, 1 dynamic)".

## 5. Cross-drawable transforms and links (REQ-03)

There are THREE independent spaces:
1. DrawableLocal(d): decoded vertex coordinates of drawable d.
2. RigCanvas: canonical model canvas where bones, constraints and
   all solver target positions are evaluated.
3. ViewportScreen: camera, zoom, pan, clipping and DPI-only
   presentation/picking coordinates.

Define:
  C_d = RigCanvasFromDrawable(d)
  V = ScreenFromRigCanvas(camera, viewport, DPI)
  p_screen = V * C_d * p_local
  p_local = inverse(C_d) * inverse(V) * p_screen

C_d MUST be obtained from a validated model adapter, never guessed
from per-drawable bounding boxes. If all current Core drawable
positions are already in model-wide canvas units, adapter C_d is
EXPLICIT IDENTITY; add parity tests rather than inserting a second
transform. Reject nonfinite, noninvertible or mismatched C_d.

Cross-drawable transform parenting:
- All bones in one skeleton group share RigCanvas even when
  their owner_drawable differs. Parent/child can cross drawable
  boundaries only by explicit "bone.attach" with two stable BoneIDs,
  no inferred geometry or semantic relationship.
- Parent must be unique; full global skeleton DAG cycle check,
  including edges across drawables.
- A cross-drawable bone inherits its parent's PoseWorld; it does
  NOT inherit arbitrary renderer camera transforms or render order.
- Legacy V4 cross links remain XPBD vertex-distance joints,
  converted to shared canvas through C_d and C_target. They do
  not become bone parent transforms or automatic skin weights.
- Drawing/render order and part opacity remain original Core
  metadata unless a separately approved feature modifies them.
- Pick uses inverse viewport transform, scene depth/layer order and
  a bounded tolerance in logical screen pixels; invisible/locked
  layer picks are denied by explicit editor settings.

Proposed test reference using ONLY synthetic geometry: drawable A
has C_A identity; drawable B has translation (3, -2) and unit scale;
bone in A rotates +90 degrees; a child of that bone anchors
drawable B without jumping in bind pose. Separate reflected/
noninvertible and mismatched generation cases reject.

## 6. Animation -> V5 skin -> XPBD -> render (REQ-04, REQ-06)

Proposed FrameCoordinator frame transaction:

    F0. Latch SessionEpoch, ModelGeneration, DocumentRevision,
        PhysicsRevision, CameraRevision, animation time and dt.
    F1. On owner thread, evaluate Core animation + linked Physics3;
        csmUpdateModel once; capture immutable animated positions A(t).
    F2. Evaluate validated bone PoseWorld DAG from V5 immutable snapshot.
    F3. SkinningEvaluator generates finite, bounded target canvas
        positions, then converts them to per-drawable local coordinates.
    F4. XPBDAdapter validates selection/topology/budgets and passes
        skinned target scene to solver.prepare; candidate only.
    F5. Compose RenderCandidate using candidate vertex pointer arrays
        and unchanged validated source texture/UV/catalog.
    F6. Submit render and verify required native render completion
        fence; do not accept solver on speculative submission.
    F7. Recheck F0 session/model/doc/physics revision, no stale handle,
        finite positions and eligible renderer state; ACCEPT candidate
        and publish AcceptedFrame generation in one owner action.
    F8. Emit typed frame/commit receipt and private-safe diagnostics.

Implementation seam:
- Current studio_xpbd_prepare extracts model positions from csmModel.
  V5 should add an INTERNAL typed ScenePose input or immutable
  SceneSnapshot adapter (not a public ABI change), and direct XPBD
  source_motion_transfer/pose constraints toward the SKINNED target.
- Current studio_xpbd_positions substitutes pointer arrays for render;
  preserve a view lifetime until render fence/accept completes.
- Avoid mutating Core source buffers then "restoring" them; this
  creates hidden double transform and failure cleanup hazards.
- Avoid rendering with a global active pointer that survives
  model reload, solver disable, aborted frame or stale candidate.
- Current native XPBD still limits 2 drawables/2048 total vertices;
  offer read-only rigs for other drawables and an explicit
  UNSUPPORTED_SOLVER_SCOPE status rather than partial simulation.
- Physics pins/cross joints are all evaluated in consistent canvas
  units. If solver remains internally in drawable units, require
  a verified inverse conversion at the adapter boundary.

Time integration:
- Use explicit simulation dt and remaining-step accounting; pause
  does not advance physics, single-step has one deterministic tick.
- On commit of a new rig pose, transform topology or rig apply,
  choose controlled rebind/reset of solver candidate. Do not
  reinterpret old solver velocities in a new coordinate frame
  without deterministic transfer logic and RED-first tests.
- Preserve linked Physics3 parameter-stage behavior distinct from
  native mesh XPBD; one source animation pass, no double stepping.
- A rejected candidate must leave previous AcceptedFrame and solver
  revision unchanged, emit an error and keep source asset unchanged.
- If GPU errors after any pixels became visible, software
  accepted-state atomicity does NOT prove perfect display atomicity.
  Avoid swap/presentation of unaccepted candidates where backend
  permits, and record this as a physical rendering gate.

Performance budgets (PROPOSED study targets, not measurements):
- Benchmark CPU DAG evaluation, four-weight LBS, XPBD solve,
  GPU submission, inspector painting at current solver cap.
- Target p95 UI interaction latency <=50 ms for bounded synthetic
  rig edits, 60 FPS at a chosen qualified model/machine only AFTER
  physical measurement. No FPS claim is made by the design.
- Use preallocated pose/influence/vertex buffers, recompute
  topological order and inverse-bind only on document commit,
  dirty-region brush preview and bounded event queue.
- Measure memory peak, allocations/frame, physics step, GPU fence,
  frame-drop/rebind spikes and model reload safety before optimizing.

## 7. Undo / redo / versioned edit transactions (REQ-05, REQ-07)

States:
    IDLE -> DRAFT -> VALIDATED -> COMMIT_QUEUED
       -> COMMITTED / ABORTED / UNKNOWN_OUTCOME

Atomic unit = one user logical operation, e.g. one brush stroke,
one reparent, one batch influence edit or one pose manipulation.
Editor edits are not equivalent to accepted XPBD frames.

Local UndoHistory entry:
  transaction_id, request_id, session_epoch, model_generation,
  base_revision, resulting_revision, op_name, affected entity IDs,
  inverse_delta, forward_delta, private local verification metadata.
Never contain source textures, model bytes or public derived geometry.
Use bounded undo/redo inverse diffs; snapshot/checkpoint optimization is
OPTIONAL after measurements. On memory cap, reject new edits safely;
never silently truncate an active undo transaction.

Exact workflow:
1. draft.begin(if_revision=r) grants ONE writer lease for current model;
   concurrent UI and Agent editors use same admission/CAS.
2. Mutations are applied to a private copy-on-write document candidate.
   Preview may update a temporary overlay but not the active solver.
3. draft.validate performs full DAG, influence, identity, physics budget,
   coordinate and persistence rule checks; no state mutation on error.
4. draft.commit checks (session, model_generation, revision) CAS,
   publishes new RigDocument revision and schedules an apply frame.
   This is an editor COMMITTED receipt, NOT an XPBD APPLIED receipt.
5. physics.apply(version=r+1) emits APPLIED only after eligible solver
   and frame accept; otherwise document remains valid but unapplied,
   with a visible Unapplied/Failed badge and previous solver retained.
6. undo targets only the latest committed editor transaction in the
   current revision lineage. Compute and validate its inverse as NEW
   edit revision; do not time-travel to a stale session revision.
7. redo targets the latest undone transaction only if no divergent
   new edit occurred. Any new committed edit invalidates redo branch.
8. Save snapshots the validated document at an explicit revision using
   same-volume temp write + flush + durable replace + readback.
   Save is not an implicit solver apply, and undo after save marks dirty.

Crash and ACK-loss:
- Draft is lost on crash; committed document survives only after an
  explicit successful local save. Do not require a durable command journal
  or promise exactly-once arbitrary UI/GPU effects.
- Request ID deduplication is scoped to session_epoch; unknown ACK
  requires request-status readback before user/agent retries.
- Orphaned lock and pending files cannot be claimed/deleted by a
  different identity without verified recovery protocol.
- Undo/redo after model topology or generation change is REJECTED
  until explicit reload/rebind decision, not best-effort remapping.

Recommended UI statuses: Draft, Validated, Committed (document),
Applied (solver frame), Save Pending, Saved, Conflict, Rejected.
Never show "Saved" on queued I/O or "Applied" on receipt-only ACK.

## 8. UI/UX: full Rig Editor workspace (REQ-08)

Use a dedicated "Rig Editor V5" workspace alongside existing Studio,
not additional controls grafted onto the current ~316px right panel.
Maintain current Preview/Params/Layers/XPBD modes as legacy fallback.
Responsive min viewport; dockable inspector, left scene tree,
center canvas, bottom timeline/events and right property panel.

Conceptual wireframe (not a screenshot or implementation):

    +-----------------------------------------------------------------+
    | Model session | Rig V5 OFF/ON | revision | Draft/Applied | Save |
    +----------------+--------------------------------+----------------+
    | Scene tree     | viewport (2D avatar)           | Inspector      |
    | - Drawables    |  selectable bones/joints       | Bone TRS       |
    | - Components   |  weight heatmap/brush          | Parent / Pivot |
    | - Skeletons    |  cross-layer joints            | Skin weights  |
    | - Bones        |  canvas-space gizmos           | Lock / Normalize|
    | - Constraints  |  camera pan/zoom               | XPBD settings |
    +----------------+--------------------------------+----------------+
    | Select | Bone | Parent | Weight Paint | Constraint | Sim Preview |
    +-----------------------------------------------------------------+
    | Timeline / animation scrub | Undo Redo | Errors | Events | Diff |
    +-----------------------------------------------------------------+

Modes and interaction rules:
- Select: click joint/bone/drawable; Ctrl multi-select; Shift range
  for scene tree only; deterministic hit priority and no implicit
  constraint edit. Locked/hidden layers have non-pickable overlay.
- Bone Create: two explicit points establish head/tail or origin/
  rest direction, draft bone appears with ID and parent chooser;
  no geometry-based auto-parenting.
- Bone Parent: select child and parent in tree/viewport, show preview
  arrow, validate DAG before enabling Commit; choose Preserve World
  or Preserve Local as explicit toggle.
- Pose: translation/rotation/scale handles with numeric editable
  fields in RigCanvas units/radians or displayed degrees with
  documented conversion. Separate Bind Edit vs Pose Preview modes.
- Weight Paint: choose target bone, brush mode, radius, strength,
  falloff, locks and heatmap; brush stroke one undo item.
  Include Numeric Weights and Normalize table for precision.
- Cross-Drawing: pair of explicitly named source and target
  drawables, side-by-side coordinate visualizer, identity of each
  C_d and a preview of attachment transform. Never connect based
  solely on proximity or semantic hair/skirt guess.
- Constraints: tab for V4 XPBD anchors/beams/parent-distance and
  cross joints, visually distinct from bone links. Legacy scalar
  physics mobility has a distinct heatmap and legend.
- Simulation: Pause/Step/Reset, Toggle XPBD, source vs skinned vs
  simulated ghost overlay; native solver red error overlay on
  rejected candidates, not silent spring fallback.
- Timeline: record no automatic skeletal keyframes in initial V5;
  pose manipulations are still undoable. Timeline scrubbing may
  preview existing Core animation without authoring motions.
- Inspector: explicit owner drawable, bone ID, parent, TRS,
  assigned vertex influence list, normalization sum, revision,
  pending apply and read-only diagnostics.

Key bindings (proposed):
  V select; B create bone; P parent; W paint; E constraints;
  Ctrl+Z undo; Ctrl+Y or Ctrl+Shift+Z redo; Ctrl+S save;
  Space pause/play; . one deterministic simulation step;
  Esc cancel draft/drag (never cancel committed transaction).
  Prevent shortcut capture during text editing, composition/IME
  and modal dialogs. Provide a shortcut reference and remapping
  as later accessibility work.

UX and accessibility:
- Visible labels and icons, color plus shape for weight/lock state,
  keyboard focus navigation, high-DPI-scaled hit target, safe
  scrolling at narrow widths and tooltips for disabled actions.
- UI preview never exposes private model/image data in public logs
  or automatic screenshots; captures stay ignored and Owner-local.
- Distinguish bind pose (neutral), source animated pose, bone-skinned
  pose and XPBD-accepted pose using explicit legend and toggles.
- Changes to pose/weights display Draft until applied; no green
  success banner from a mere command ACK.
- Agent changes trigger a revision event; if a human has a draft,
  show conflict, retain draft for read-only diff and offer explicit
  discard/rebase (no silent overwrite).

## 9. Agent Toolkit API v2 integration (REQ-08/09)

V5 tool namespace is DECLARATIVE and NOT currently callable:

| Operation | Side effect | Preconditions / result |
|---|---|---|
| avatar.capabilities / avatar.schema | read | advertised V5 flag/limits/schema |
| rig.document.inspect | read | revision, applied revision, redacted summary |
| rig.document.begin | draft | expected session/model/edit revision |
| rig.document.validate / diff | read draft | structured errors, privacy filter |
| rig.document.commit / abort | edit | one writer lease, CAS and receipt |
| rig.document.undo / redo | edit | current lineage, inverse/forward CAS |
| rig.bone.create / inspect / reparent / pose | draft/read | stable IDs, acyclic, affine validity |
| rig.skin.weight.set / paint / normalize | draft | <=4 bones/vertex and lock policy |
| rig.constraint.upsert / remove | draft | V4 independent typed constraints |
| rig.spaces.inspect | read | public-safe transform capability only |
| rig.preview.evaluate | private read | bounded temporary preview, no commit |
| rig.physics.apply / status | mutate/read | accepted frame generation after fence |
| rig.document.save / recover | mutate/read | fixed local storage, durable readback |

All v2 commands use ADR-002 envelope:
  api_version, request_id, session_id, expected_model_generation,
  expected_edit_revision, operation, arguments, mode.
Extra fields and type coercion are rejected. Native-side authoritative
validator prevents bypass of Python client. UI sends the SAME typed
commands to EditorDocumentStore; no alternate unsafe write path.

Synthetic example, not executable now:

    {
      "api_version": 2,
      "request_id": "req-v5-example",
      "session_id": "synthetic-session",
      "expected_model_generation": 7,
      "expected_edit_revision": 21,
      "operation": "rig.bone.reparent",
      "mode": "stage",
      "arguments": {
        "document_id": "rig-synthetic",
        "bone_id": "bone-child",
        "new_parent_id": "bone-root",
        "preserve_world": true
      }
    }

Staged receipt has draft_revision; committed receipt has
document_revision; applied receipt MUST include accepted_frame_generation
and solver_revision. Never call all of them "ok=true" without state.
Caller with no V5 EDIT capability gets PERMISSION_DENIED; read-only
inspection uses filtered metadata; unknown tool schema gets DENY.
Tools do not accept arbitrary paths, model assets, URLs, shell commands,
native libraries or image bytes.

Computer Bridge H5 integration is OPTIONAL and outside Rig Editor V5
acceptance. The Studio UI and local Agent API are the supported control
surfaces. Local authorized geometry readback is needed for skinning/rig
inspection; keep raw model data local and separate from public test reports.
Same-user Windows processes are not isolated by the IPC layer.

## 10. RED-first verification matrix and acceptance (REQ-10)

Run each negative oracle RED on intentionally defective prototype,
or otherwise demonstrate that it catches a controlled mutation.
Then implement the smallest permitted change, run GREEN on exact
Windows binaries and synthetic fixtures. Never weaken oracle.

| ID | Scenario | Required oracle |
|---|---|---|
| GRAPH-01 | missing parent, duplicate ID, self-parent, cycle | full rejection; zero doc delta |
| GRAPH-02 | cross-drawable cycle / reparent preserves world | reject cycle; preserved world within tolerance |
| GRAPH-03 | parent change with shear/noninvertible matrix | reject with no partial mutation |
| GRAPH-04 | 3-bone hierarchy with rotations/translations | deterministic matrices and ordering |
| BIND-01 | identity pose on synthetic source animation | skinned exactly follows source pose |
| BIND-02 | 90-degree rotation around explicit synthetic joint | analytically expected transformed positions |
| BIND-03 | nonlinear source animation + additive skin | no double deformation; finite candidate |
| SKIN-01 | 4 weights sum to 1 / 5th influence | normalized or REJECT, never silent loss |
| SKIN-02 | NaN/Inf/negative/duplicate/zero weights | reject or documented canonical cleanup, no mutation on error |
| SKIN-03 | locked bones + 100 brush samples | locked unchanged, one undo transaction |
| SKIN-04 | disconnected islands and Smooth brush | no cross-island bleeding |
| SPACE-01 | two synthetic drawables, translated C_d | expected canvas/pixel/inverse pick result |
| SPACE-02 | singular/reflected/unavailable C_d | explicit reject/unsupported; no frame publish |
| SPACE-03 | cross bone attachment vs V4 cross joint | separate semantics and same-canvas endpoints |
| FRAME-01 | model reload after prepare before accept | stale candidate rejected, old solver not advanced |
| FRAME-02 | document edit after candidate preparation | CAS reject/recompute, no mixed-generation frame |
| FRAME-03 | XPBD budget/nonfinite/rebind failure | unchanged last accepted state and clear UI status |
| FRAME-04 | GPU fence/render failure / late ACK | no false APPLIED receipt; UNKNOWN_OUTCOME where applicable |
| FRAME-05 | pause/step and linked Physics3 + XPBD | no double step, deterministic time/accepted revisions |
| EDIT-01 | undo/redo reparent + weight stroke | exact private state round-trip, increasing revisions |
| EDIT-02 | UI and Agent concurrent writes | single CAS winner, conflict no lost update |
| EDIT-03 | redo after divergent edit | redo branch invalidated; no hidden restoration |
| EDIT-04 | crash during draft/commit + stale response | no half document, explicit recover/unknown |
| FILE-01 | V1–V4 load + V5 opt-in read | legacy behavior unchanged; V5 gated |
| FILE-02 | corrupt V5, duplicate keys, truncation, disk full | fail-closed, previous durable doc intact |
| FILE-03 | save rename denied / crash/restart | temp recovery & previous-good load physical evidence |
| UI-01 | drag stroke, cancel, keyboard, high-DPI pick | one edit, cancel no delta, correct selected handle |
| UI-02 | V4 vs bone control & conflict status | no semantic ambiguity, all statuses truthful |
| PRIV-01 | model IDs/bytes/path/hash/image in public reporter | filtered/denied, no sensitive artifact |
| API-01 | malformed native opcode/overflow/extra suffix | ERR before mutation across every opcode |
| API-02 | two agents/ACK loss/replay/restart | owner-safe request identities, no false exactly-once |
| PERF-01 | synthetic 2-drawable/2048-vertex/128-bone load | measured bounded work; no claims without data |
| REG-01 | v1 Agent contract and ARS ABI20 exports | no changes/regressions on exact pinned build |

Acceptance tiers:
T0 design/schema lint only; T1 synthetic CPU/contract tests; T2
fresh Windows Native Studio + GUI physical; T3 cross-platform
(where applicable), long-run/perf, crash/restart and privacy custody;
T4 independent review/Owner signoff. Passing T0/T1 does not release V5.

Reference numerical tolerances to freeze before implementation:
- normalized sum |sum(w)-1| <=1e-5; no duplicate influences.
- matrix/expected coordinate absolute error <=1e-5 model unit
  on small synthetic unit-scale fixtures, plus scale-relative error
  for large/small bounds; conversions float64->float32 checked.
- no NaN/Inf, no stale publish, all error cases no state delta.
These are PROPOSED test tolerances, not observed measurements.
Sample 90-degree transforms should compare against analytic vectors
rather than screenshots alone.

## 11. Migration, rollout and rollback

M0. Preserve immutable branch/working tree evidence, mark ADR-001/002
    PROPOSED and collect correct exact-HEAD test logs. No conflict
    resolution by resetting concurrent changes.
M1. Complete ADR-002 G1 strict native grammar, G2 exclusive IPC and
    G3 private evidence/persistence gates with Windows RED->GREEN.
M2. Introduce V5 schema/validator in tests-only fixture; ensure V1–V4
    load parity and rejects for invalid V5. NO live mutation.
M3. Implement pure bone DAG, space and skinning CPU library;
    no GUI, no private fixture export. Synthetic GREEN first.
M4. Implement draft/CAS/undo/redo and private durability with
    crash/fault injections and deterministic event receipts.
M5. Integrate frame adapter to native XPBD/renderer under V5 OFF
    default and explicit Developer opt-in; fresh physical tests.
M6. Build editor workspace UI with keyboard/brush/inspector and
    private GUI acceptance. Agent v2 read-only first, then gated edit.
M7. Owner acceptance, independent review and local performance targets.
    Any Computer Bridge adapter is a separate optional workstream; no
    ABI20 widening, compulsory external integration or implicit release.

Migration rules:
- V1–V4 files remain readable, and saving legacy mode writes V4.
- V5 import starts as a COPY of V4 constraints plus an EMPTY bone
  graph/skin table; never auto-convert V4 vertex parents to bones.
- A V5 sidecar does not overwrite an existing V4 file; retain
  prior version and recovery pointer. V5 reader rejects partial
  construction and unknown major version atomically.
- Switching V5 feature flag OFF restores unmodified V4 runtime path,
  clears transient V5 candidates and does not silently discard
  unsaved V5 edits; offer export/save private snapshot first.
- No V5 data is published to remote Git, CI or issue attachments.
  An Owner-approved data custody review is necessary before any
  legitimate public sample export.

## 12. Risk and open decision ledger

| ID | Decision / risk | Recommended provisional choice | Gate |
|---|---|---|---|
| D01 | Core animated mesh + skin blending | additive bind-relative delta | SKIN/BIND T1 |
| D02 | Canonical cross-drawable transform | explicit C_d (identity only if proved) | SPACE T1+T2 |
| D03 | Pose/bind TRS and pivot | joint-origin, local TRS, preserve-world reparent | GRAPH T1 |
| D04 | Reflections and shear | reject unsupported transforms in beta | SPACE T1 |
| D05 | Bone and vertex budgets | 128 bones total, <=4 weights, keep native XPBD 2048/2 cap | PERF T2 |
| D06 | Persistence | versioned V5 sidecar + atomic same-volume replace | FILE T2 |
| D07 | Undo granularity | one logical stroke/operation, CAS/new revision | EDIT T1+T2 |
| D08 | Physics state on rig revision change | controlled reset/rebind; no guessed velocity transfer | FRAME T1+T2 |
| D09 | Native render fence | use qualified GPU/query completion gate; no ACK-only apply | FRAME T2 |
| D10 | Local process trust | trusted-local only; broker/stronger isolation OUT OF SCOPE; validate IPC & paths | Local API review |
| D11 | Human UI layout | dedicated dockable Rig Editor workspace, legacy intact | UI T2 |
| D12 | Release | feature OFF until G1–G3, T1–T4 and Owner approval | RELEASE HOLD |

MA context assessment: coexistence with V1–V4 is necessary; strict
conversion only in an explicit copy/import workflow; new V5
bone/skin ownership must not absorb legacy physics source ownership;
new GUI must not bypass single-owner command dispatch; test and
rollback gates precede a production migration. No forced rewrite
of the broader ARS SDK, host graphics backend or Computer Bridge.

Design exit: document provides precise interfaces, coordinate and
numerical conventions, module ownership, transaction semantics,
migration and RED-first oracles. DESIGN DELIVERED is not
IMPLEMENTATION PASS; only Owner may promote this proposed ADR.

## Source references (local repository)

S01 docs/in-progress/studio-beta-20261023/ADR_PROPOSED_HIERARCHICAL_SKINNING.md
S02 docs/in-progress/studio-beta-20261023/ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md
S03 tools/avatar-studio/studio_rig.h / studio_rig.c
S04 tools/avatar-studio/studio_xpbd.h / studio_xpbd.cpp
S05 tools/avatar-studio/native_mesh_xpbd.hpp
S06 tools/avatar-studio/studio.c / studio_agent_extended.inc
S07 scripts/studio_agent_api.py / tools/avatar-studio/AGENT_API.md
S08 docs/BETA_EXPANDED_QUALITY_SECURITY_ARCH_AUDIT_20261023.md
S09 docs/in-progress/studio-beta-20261023/RED_FIRST_VERIFICATION_MATRIX.md

End. DESIGN-ONLY / PROPOSED / HOLD.

## Appendix A — Local-only resolution and UI/API parity audit (2026-10-23)

Scope: one Studio instance and local client(s) on one machine; local data
model/rig access is allowed for authorized editing. No remote or cloud API,
broker, multi-host sync, dynamic plugin manager, or mandatory H5 adapter.
The eight conceptual rig_v5_* modules are source-code ownership boundaries,
not eight services or processes. Retain internal correctness properties:
model identity/topology, finite transforms, frame generation fence,
undo/redo, local persistence, bounded IPC and explicit apply status.

Current Studio API v1 supports 52 declared Native commands, but lacks:
(1) model.open/reload/close and enumerating active model(s);
(2) paged parameter/part lists and ID-to-index lookup;
(3) bounded per-vertex positions/triangles, drawable transforms and all
    constraint endpoints/weights necessary for Rig V5 editing;
(4) complete camera, gaze, interaction and applied-frame state readbacks;
(5) local Undo/Redo transaction APIs and live skeletal V5 tools.
The V2 prototype exposes only two offline discovery operations. These gaps
are source-audited and should not be called physical E2E failures.

Tool overlaps: physics.set / xpbd.set (shared gravity/wind/damping);
rig.select / xpbd.select (shared drawable with different UI effects);
rig.apply / xpbd.apply (two different solver bind paths, NOT equivalent).
Avoid removing or unifying commands until behavioral equivalence and v1
compatibility are independently demonstrated. UI/Agent should share a local
command bus and a bounded revision/readback contract, not a distributed
journal. This appendix supersedes overly broad references to mandatory
separate broker/H5 integration and public-report restrictions on trusted
local geometry inspection. It does NOT supersede local security, XPBD
atomic-frame or model privacy requirements for exported artifacts.
