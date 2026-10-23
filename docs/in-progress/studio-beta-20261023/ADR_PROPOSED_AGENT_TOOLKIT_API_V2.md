# ADR-002 — Avatar Runtime Agent Toolkit & Studio API v2

- Date: 2026-10-23
- State: PROPOSED / DESIGN-ONLY / LOCAL-ONLY SCOPE / RELEASE HOLD
- Target: Avatar Studio local process + local Agent client, Beta workspace; Computer Bridge is optional development transport, not a runtime dependency
- Baseline inspected: HEAD 05df4d374414a6062e69f766eb0b42f25be885b8, dirty working tree
- Authority: design handoff; no production/runtime source change, no public SDK ABI change, no merge/release permission
- Privacy: PUBLIC-SAFE DESIGN. No real model paths, bytes, images, coordinates, derived hashes or private evidence.

## 1. Problem and observed baseline

The current experimental tools/avatar-studio/AGENT_API.md describes JSON -> Python
validator -> ASCII file request -> Native Studio -> text receipt, version 1.0.
scripts/studio_agent_api.py has a hard-coded operation/field map and command
formatter. tools/avatar-studio/studio.c contains request dispatch, UI, model
state and frame processing; studio_agent_extended.inc holds further handlers.
studio_rig.h defines V4 anchors, beams, parent-distance constraints, sparse
weights and cross-drawable distance links. These are NOT skeletal bones or
multi-influence skinning. The SDK-facing include/ars/ars.h is a separate
ABI design/implementation boundary and is NOT the Studio Agent transport.

Computer Bridge currently exposes H5 manifest v2 in legacy mode, with six
fixed groups: control, workspace, files, terminal, desktop, browser. It does
NOT expose a registered Avatar-specific group. Terminal calls to the Studio
CLI are a development workaround, not a scoped Avatar permission boundary.

Audited historical findings at e38eba5: F01 strict native grammar, F02
shared-request IPC, F03 private unreachable Git objects, F04 output privacy,
F05 rig durability, F06 controller coupling, F07 coverage. The current dirty
tree contains candidate changes to native EOF checking, exclusive cooperating
writer.lock, and temporary/replace rig saves. Six Python API unittest cases
PASS on current tree. These changes are NOT independently certified and do
not retire the historical physical findings without fresh builds, negative
E2E, crash and concurrency tests. Release gate remains HOLD.

## 2. Decision

Introduce a separately versioned, opt-in Avatar Studio Agent Toolkit v2,
with a schema-driven local API, typed command router, transactionally staged
editor documents and a single native model/frame owner. Keep Studio v1 and
ARS public C ABI source-compatible. No arbitrary shell, file path, model
export or network service will be added to the tool surface.

Use one local command dispatcher shared by the Studio UI and local Agent client.
For Beta, retain the existing bounded file IPC while its native/parser, writer
ownership, timeout and stale-response gates are incomplete. A Windows Named
Pipe is an OPTIONAL replacement when measured or verified requirements justify
it, not a second service by default. Do not add remote serving, distributed
identity or a separate OS-principal broker to the Local-only deliverable.
Local IPC is not an OS sandbox against other same-user processes. Keep a
single checked mutation path for both v1 compatibility and v2 commands.

All externally supplied commands must enter one strict typed validator:
envelope -> opcode schema -> semantic validation -> model generation and
edit revision check -> staged change or read snapshot -> UI-thread commit
-> typed receipt. The native validator is authoritative even if a Python
wrapper has validated the same command.

## 3. Boundaries and ownership

    Human Studio UI / local Agent client (same host, trusted)
                  |
       Avatar Toolkit catalog + schema + admission
                  |
      One Studio-owned dispatcher + bounded local IPC
                  |
      Strict typed parser / dispatcher / authorization predicates
           |                                  |
      Read-only immutable snapshot         Draft editor document
           |                                  |
       filtered result           validate -> commit at frame boundary
                                              |
        model generation -> animation -> optional bone/skinning
            -> XPBD -> finite/checkpoint validation -> render

- ModelLoader: original files, textures and model geometry are immutable.
  Only a UI/Owner-authorized model session may be addressed. No arbitrary
  input path passed through Agent commands.
- SessionManager: opaque session ID, editor lease, model generation, operation
  and schema capabilities; it never treats same-user IPC as authentication.
- CatalogService: internal IDs, model parameters, parts, drawable components;
  private data classification controls every response and log.
- RigDocument: staged editable constraints, labels, anchors, weights and
  versions. V4 remains readable. It does not own the live physics solver.
- SkeletonDocument / SkinningEvaluator: optional V5 proposal from ADR-001;
  bone transforms and at most four bone influences per vertex. Not an alias
  for rig.parent distance constraints.
- PhysicsService: explicit option validation and deterministic rebind; owns
  accepted solver state. No source mesh modification.
- FrameCoordinator: one mutable model/UI/render thread, generation-fenced
  atomic frame acceptance. Workers can build immutable proposals only.
- PersistService: same-volume temporary file + flush + replace, version
  validation, recovery state and previous-good fallback where feasible.
- LocalDiagnostics: model-related inspection stays in the local process or
  local workspace; only expressly exported reports use the public-safe
  allowlist. No standalone EvidenceService is required.

## 4. Contract v2

Client calls first discover versions and capabilities. Reject unknown fields,
non-finite numbers, integer overflow, extra tokens, invalid UTF-8, unknown
opcodes, stale session/model generation and stale edit revision.

Request example (synthetic identifiers, not from a real model):

    {
      "api_version": 2,
      "request_id": "req-example-001",
      "session_id": "session-example",
      "expected_model_generation": 7,
      "expected_edit_revision": 21,
      "operation": "rig.constraint.upsert",
      "arguments": {
        "drawable": 0, "kind": "parent_distance",
        "parent_vertex": 1, "child_vertex": 2, "stiffness": 0.8
      },
      "mode": "stage"
    }

Response contract:

    {
      "api_version": 2,
      "request_id": "req-example-001",
      "state": "staged",
      "result": {"draft_revision": 22},
      "error": null
    }

Every mutating call has an explicit state: REJECTED, STAGED, COMMITTED or
UNKNOWN_OUTCOME. An ACK for queued work is not a claim that a frame applied
it. Error codes include INVALID_ARGUMENT, UNKNOWN_OPERATION, UNSUPPORTED,
BUSY, TIMEOUT_UNKNOWN, STALE_SESSION, STALE_MODEL, REVISION_CONFLICT,
OUT_OF_BUDGET, PERMISSION_DENIED, INVALID_DRAFT, COMMIT_FAILED,
PERSIST_FAILED and PRIVATE_OUTPUT_DENIED. Generic errors must not reveal
local private paths or model-derived payloads.

Proposed limits, subject to benchmark and formal selection: <=64 KiB JSON
request, <=256 KiB private response, hard finite scalar bounds, bounded
queue and deadline, max 1 writer transaction per model session. Capabilities
return the actual supported limits; do not silently assume 2048 vertices,
two drawables or V5 availability across all models. Public-report output excludes private model geometry, IDs, images and textures.
The trusted LOCAL editing API may return bounded per-vertex geometry, topology,
model IDs and transforms when requested for rig/mesh editing; these are local
responses, not public artifacts. Do not impose a blanket public-safe filter on
legitimate local rig inspection. Enforce request limits and never auto-export.

## 5. Agent toolkit categories (proposals, not callable today)

| Namespace / tools | Effect | Purpose / condition |
|---|---|---|
| avatar.capabilities / avatar.schema | Read | Version negotiation; searchable operations; limits; permissions |
| session.open / status / close | Mixed | Owner-approved model session; close and stale-generation fences |
| scene.inspect / snapshot | Read | Frame/playing/selection and private-safe aggregate snapshots |
| catalog.parameters / parts / drawables | Read | Bounded pagination, filtered discoverability; no raw assets |
| layer.components / relations / propose | Read | Geometry topology aids; no automatic semantic assertion |
| layer.adopt / label | Stage | Explicit acceptance of inferred anchors or human-confirmed roles |
| rig.inspect / constraint.upsert / constraint.remove | Mixed | Anchors, beams, parent-distance and cross constraints |
| rig.weight.paint / batch / inspect | Mixed | Masked brush, bounded sparse edits, undo preview |
| bone.create / parent / pose / inspect | V5 opt-in | Actual bone DAG, transforms, cycle prevention |
| skin.bind / weight.paint / inspect | V5 opt-in | Normalized multi-influence vertex skinning |
| physics.config / inspect / simulate / apply | Mixed | XPBD options; staged deterministic live rebind |
| animation.play / pause / step / expression / parameter | Mixed | Typed animation commands and parameter pins |
| viewport.camera / select / capture | Mixed | UI controls; capture remains private, never binary result |
| draft.begin / validate / diff / commit / abort / undo | Mixed | Versioned reversible transaction, effect readback |
| project.save / load / revision / recover | Mixed | Local fixed-root, explicit durability/recovery |
| events.poll / diagnostics.summary | Read | Sequence-bearing state changes, generic health and error classes |

Command namespaces must not depend on a GUI tab or a selected vertex hidden
in mutable global UI state. The target and generation are explicit; brush
must carry an explicit center handle and source coordinate-space identifier,
validated against the active immutable model. Read-only inspection and
state-changing adoption must be different operations.

## 6. Local IPC and editor-state consistency

1. One Studio process owns the model, mutable document and render frame.
   UI and local Agent enter the same validated command path.
2. Keep request ID correlation, exact payload parsing, bounded queues and
   a single in-flight writer/mutation. BUSY and timeout never imply success;
   cleanup must not delete a foreign request or response.
3. Use model generation and document revision only where a command needs
   protection against model reload or conflicting UI edits. Undo/redo is
   an editor history, not a distributed transaction log.
4. Commit edited document changes on the owner thread and distinguish
   queued, committed document and applied solver/frame state.
5. On lost ACK, report UNKNOWN_OUTCOME and require readback before retry
   of non-idempotent commands. On local Studio restart, invalidate stale
   commands and abandoned drafts. A durable replay/request journal,
   distributed sessions and multi-host coordination are OUT OF SCOPE.
6. Keep same-user threat limitations explicit: process-local / IPC
   validation cannot substitute for OS file permissions. No separate
   OS broker or complex authorization service is required by this ADR.

## 7. Backward compatibility and extension policy

- Studio API v1 stays on its old entry point while v2 is experimental.
  v1 requests translate through the same strict typed core only after
  native grammar and concurrency gates pass. Deprecation is explicit.
- Keep rig V1-V4 readers. V5 skeletal data is opt-in in a separate section;
  do not reinterpret legacy rig.parent as a bone. Reject partial/invalid
  V5 loads atomically; a V4 reader must not silently accept V5.
- Do not change ARS public ABI20. New Studio editor tools are distinct
  from public runtime render/motion APIs.
- Implement a SMALL, statically registered local command catalog with schemas
  and supported-version checks. Do not build a dynamic plugin manager or
  separately deployed extension service for this scope. Unknown commands DENY.
- No arbitrary Python execution, filesystem path, URL, plugin binary
  loader, remote HTTP server or native DLL loading through Agent commands.
- Computer Bridge integration is an OPTIONAL outside adapter; it is NOT
  required to finish or test the local Studio API, and this ADR does not
  authorize registering a new H5 group.

## 8. Prioritized delivery plan and gates

G0 BASELINE: pin exact HEAD + diff; preserve the current dirty source;
classify audit F01-F07 as historical evidence with fresh retest required.

G1 SECURITY RED-FIRST: malformed opcode/token/suffix/NaN/Inf/overflow
matrix for all native mutations; verify ERR AND no state delta; fresh
Windows physical build and synthetic fixtures. No oracle weakening.

G2 LOCAL IPC: bounded one-writer admission, stale ACK, queue collision,
crash/restart, busy/timeout and response ownership. Preserve explicit
UNKNOWN_OUTCOME; do not require distributed or multi-host machinery.

G3 PERSISTENCE/PRIVACY: partial write, fsync failure, replace denied,
restart and previous-good recovery; prevent model/path/hash/count/
image data in public reports. Do not prune unreachable Git objects.

G4 V2 READ-ONLY: implement schema/capabilities, session/status/catalog,
bounded read snapshots and event revisions; Python schema/client parity
tests; retain v1 tests and ABI20 exports.

G5 V2 EDITING: draft operations, CAS revisions, effect-based verification,
undo, UI-vs-Agent conflict handling and explicit apply/commit fences. Private
Windows GUI and native integration evidence.

G6 V5 OPTIONAL: bone DAG, transform conventions, four-way normalized
skinning, cross-drawable coordinate mapping, XPBD input pose/accepted
state, deterministic synthetic mesh and numerical stress tests.
Feature OFF by default; rollback to V4.

G7 LOCAL E2E: UI/Agent parity, current-host restart/backward compatibility,
measurable frame latency and memory, and OS builds as applicable.
Computer Bridge adapter integration is a separate optional project.

Required RED-first matrix:
- syntactic: missing/unknown/extra fields, malformed input, truncated
  frame, control chars, nonfinite, negative/out-of-range, overbudget
- identity: stale model/session, mismatched generation/revision, PID reuse,
  replay request ID, two concurrent writers, ACK lost after commit
- editing: cycles, duplicate parents, invalid cross meshes, weight
  normalization overflow, sparse capacity, partial failure/undo
- persist: forced partial write, disk-full, replace denial, crash/reopen
- rendering: generation changes mid-update, stale physics state,
  frame nonfinite, pause/step ordering, UI and Agent conflict
- privacy: denied raw model data, private capture output, public logs,
  unknown extension, arbitrary path/URL attempt

Evidence levels must be distinguished: static design, unit synthetic,
fresh native physical, GUI observation, long-run/stress, independent review.
No release PASS until relevant oracles are GREEN on exact tested source
and the privacy custody gates are resolved.

## 9. Acceptance criteria for this ADR

Owner can choose v2 scope and review error/schema contracts without
altering the current source. The first implementation PR should contain
only tests and isolated transport/decoder scaffolding; it must not enable
v2 mutation before G1-G3. Proposed contract decisions left open:
- pipe versus an alternative IPC with demonstrable failure semantics;
- local IPC transport choice and bounded request correlation;
- generation/revision ownership and editor undo history limits;
- local geometry inspection budgets and public-report export separation;
- bone coordinate transforms and skin numerical tolerance;
- actual queue/payload budgets, perf targets and compatibility deadlines.

Decision remains PROPOSED / HOLD. No source patch, tool registration,
commit, push, merge, deploy or public model artifact authorized by this ADR.

## Local-only scope reconciliation / architecture audit (2026-10-23)

**Decision of scope, effective for this draft:** Avatar API controls Avatar Studio
and Native Runtime on the same Windows host. No cloud, HTTP server, remote
identity, distributed orchestration, multi-host sync, broker or dynamic plugin
registry is a condition of delivery. Computer Bridge is only an optional
client adapter, not a dependency of the API. Keep native input validation,
bounded IPC, response ownership, no false ACK and safe local file writes.

**Current observed API baseline at beta@38e7dde:** v1 contains 52 declared
operations with corresponding native dispatch branches; v2 currently only
implements offline `avatar.capabilities` and `avatar.schema` (no Native v2
control). These are declaration/source checks, not execution or behavioral
PASS evidence. The targeted gap audit found missing local model lifecycle,
parameter/part enumeration and ID lookup, bounded mesh/rig detail reads,
local UI/Agent state readback and local undo/redo. Resolve these API coverage
gaps before treating Studio feature parity as complete.

**Overlaps to preserve intentionally:** `physics.set` overlaps three keys of
`xpbd.set` (compatibility alias candidate). `rig.select` and
`xpbd.select` share XPBD drawable selection but have different UI/rig effects.
`rig.apply` and `xpbd.apply` replace the active solver using different bind
paths; neither may be merged or removed without independent state/readback
proof. Published `OK`/queued/pending does not establish accepted XPBD frame.

**Supersession rule:** The Local-only decisions here supersede any legacy
wording above implying mandatory Named Pipe, broker, request journal,
Computer Bridge registration, private-data prohibition for local geometry
inspection or multi-agent infrastructure. Security/correctness gates for
local input, physics and file persistence remain in effect. ADR-002 stays
PROPOSED / RELEASE HOLD until native validation and E2E acceptance.
