# G4 Self-Review for G5 CAS Authority — 2026-10-23

Status: SELF_REVIEW_ONLY; NOT a Codex or independent reviewer approval.
Repository: boxs-51/Avatar_Runtime. Workspace alias beta on Windows.
Branch: exp/layer-physics-native-studio-local.
Committed HEAD: 2d2fccfaeece6ec686ca09e2e44ae4230574100c.
Reviewed subject: the current uncommitted working tree, not the commit alone.
Scope: G4 revision fencing, UI / V1 Agent edit paths, IPC uncertainty and applicability to proposed G5 CAS. No model files or private coordinates exported.

## Codex access and independence

No connected Codex reviewer/action was available in this ChatGPT tool inventory. A plugin search for Codex code review did not expose an installed Codex review connector. Windows host probe through Python shutil.which('codex') returned None; gh executable was also absent. GitHub connector is available, but it cannot independently review the current uncommitted local G4 diff as a remote PR without creating/publishing a matching PR. No Codex request or review was executed. This assistant previously implemented parts of G4, so this is not independent review. G4 independent-review gate remains NOT_RUN.

## Grounded findings and severity

### G4-SR-01 — P2 for G5 CAS admission, not a proven live G4 read failure

Location: tools/avatar-studio/studio.c:41–44, 1020–1024 and 1067–1070.
The observed G4 rig_revision compares net StudioRig bytes before/after a frame phase. Two opposite logical edits within the same UI phase can leave an identical final byte sequence. The counter can therefore remain unchanged despite two successful logical operations. Conversely, bytewise structure changes need not correspond to a user-visible transaction. This is suitable for bounded-read change detection within proven cases, but cannot be elevated to the G5 document CAS revision. Missing proof: Native GUI same-frame inverse edit, actual per-logical-operation event evidence. Proposed correction: G5 document_revision increments exactly once on each accepted edit, undo or redo linearization; fail closed before overflow.

### G4-SR-02 — P2 design/gate blocker: UI, V1 Agent bypass proposed lease

Locations: tools/avatar-studio/studio.c:282–303, 420–537, 714–959; scripts/studio_agent_api.py:240–337.
UI and legacy Agent modify StudioRig through existing direct paths. The bridge writer.lock serializes command submissions but does not span draft.begin through commit and does not constrain GUI changes. Proposed G5 mutation cannot be safe with the existing authority split unless all entry points go through one owner-thread transaction dispatcher or are explicitly blocked while G5 lease exists. It is a G5 readiness finding; no G5 writer is enabled.

### G4-SR-03 — P2 design/gate blocker: lost-ACK readback path does not exist

Locations: scripts/studio_agent_api.py:286–337; tools/avatar-studio/studio.c:681–704 and 1005–1009.
The bridge creates uncertain_outcome before publishing request and preserves it when ACK is not proven. Its normal call() rejects *any* subsequent operation when the marker exists, including a hypothetical read-only draft.status request. G4/legacy ACKs do not include session/request-indexed G5 commit outcomes. A separate bounded read-only recovery lane and an owner-generated receipt index are required. Never clear unknown outcome solely because request.txt is absent.

### G4-SR-04 — Unverified GUI and local security boundary

Computer Bridge host reports USER_CONTEXT_NO_OS_SANDBOX and inherited host-user identity. Same-user IPC token and local paths are not an OS authorization boundary. G4 native Agent/Agent ABA E2E has previously been GREEN; real click-driven UI/Agent concurrent editing remains BLOCKED by desktop input gate. Do not claim adversarial same-user process isolation from input validation alone.

## Existing positives retained

- G4 generation and rig_revision are checked before/after bounded multi-command reads. Native Agent/Agent mixed snapshot and ABA tests previously rejected changed editor state.
- G4 topology/cross target bound hardening and G4_MESH_VERTEX_COUNT use a strict read-only generation-fenced Native opcode. Previous Windows physical cross tests confirmed valid target-relative child and malformed/stale input rejection.
- V2 current parser explicitly rejects session_id, expected_edit_revision and mode != read; mutating_commands_enabled=false. This is a security control to preserve while G5 schema is being proposed.
- Public SDK ABI20 is not touched by this review.

## Architecture assessment

Owner-thread Native Studio is an appropriate local transaction serialization boundary. G4 rig_revision remains a legacy read observation, whereas proposed EditorDocumentStore should own session_epoch, document_revision, draft lease, undo/redo lineage and immutable outcome receipts. These two versions have different semantics and must not be aliased.

## Project structure assessment

The Python V2 adapter is separated from the Native Studio dispatcher and public ABI20 headers. G5 should introduce a small internal transaction controller with UI, V1 compatibility and V2 Agent adapters; do not clone authorization or mutation logic into independent Python/UI paths. Native same-user security limitations remain documented. No compulsory refactor of unrelated renderer or Rig V5 code is justified.

## Optimization proposals (not approved)

OPT-01 OPTIONAL: cache immutable session/model metadata for a bounded owner command batch to reduce repeated IPC reads after correctness is established. Alternative: leave extra reads in place. No measured performance need or approval; NOT_GRANTED.

## Review outcome

Review-process mode: SELF_REVIEW_ONLY, complete as a self-review of inspected paths; independent gate NOT_RUN.
Reviewed subject: REVISE for G5 CAS use of G4 revision/legacy mutation architecture.
G4 limited readback successes: retained within previously demonstrated scope, not re-certified as full release.
G5 T2 Native writer: HOLD. No source or test changed by this review.
