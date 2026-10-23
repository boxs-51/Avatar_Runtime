# G5 Local-only CAS Transaction — Gate Assessment (2026-10-23)

**Status:** DESIGN/REVIEW ONLY · G5 T2 Native writer HOLD · no source or API activation.
**Repository/workspace:** `boxs-51/Avatar_Runtime`, Windows Computer Bridge `beta`, branch `exp/layer-physics-native-studio-local`.
**Committed HEAD:** `2d2fccfaeece6ec686ca09e2e44ae4230574100c`; G4 work is uncommitted, so the tested revision is the working tree rather than that commit alone.
**Review provenance:** assistant source/design review, NOT an independent second reviewer. ADR-002 and ADR-003 remain PROPOSED.
**Scope:** local Studio owner, Agent IPC/UI concurrent editing, CAS, post-effect ACK loss, undo/redo, accepted solver/frame. Exclude production enablement, remote service, merges, V5 bone implementation and new public SDK ABI20 symbols.

## 1. Inspected source and evidence

- `tools/avatar-studio/studio.c:35-44, 681-704, 1020-1024, 1067-1075`: per-frame `memcmp(before, s.rig)` revision increments after Agent and UI phases. Direct UI Rig V4 mutations remain active at `studio.c:282-303, 420-537`; V1 Agent Rig mutations at `studio.c:714-959` remain separate from hypothetical G5 transactions. Model load resets `rig_revision` and issues a new `model_generation` at lines 121-122.
- `scripts/avatar_toolkit_v2.py:39-63, 70-120`: V2 rejects any non-null session/edit revision and any mode other than `read`; capabilities explicitly say `mutating_commands_enabled=false`.
- `scripts/studio_agent_api.py:240-337`: cross-process byte-range `writer.lock` serializes individual client IPC submissions, NOT document draft ownership. On unknown ACK `uncertain_outcome` is retained; every subsequent ordinary call on that queue rejects if marker exists. A `draft.status` operation cannot simply reuse the blocked `AgentBridge.call()` path.
- `tools/avatar-studio/studio.c:681-704, 1005-1009`: Native tries to publish pending ACK without repeating the command, but has no G5 session-scoped outcome receipt index that can settle post-effect lost ACK.
- `tests/moc3/g5_reference_model.py` is a pure in-memory state machine (no Studio/native writer, no UI command bus). `tests/moc3/g5_redfirst_contract.py` and `g5_redfirst_lifecycle.py` are opt-in intentionally RED contracts and do not prove Native behavior.
- `docs/in-progress/studio-beta-20261023/ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md` and `ADR_003_RIG_EDITOR_V5_DETAILED_DESIGN.md` both propose one-owner CAS, but the former uses `draft.*` and the latter `rig.document.*`; proposed response states also differ in case/enumeration.

### Observed tests (no Native writer)

1. `python -B -m pytest -p no:cacheprovider -q tests/moc3/g5_reference_model.py --tb=short`: **11 PASS**, proof tier SYNTHETIC.
2. `python -B -m pytest -p no:cacheprovider -q tests/moc3/g5_redfirst_contract.py tests/moc3/g5_redfirst_lifecycle.py --tb=no`: **24 intentional RED / 6 control PASS**; RED primarily indicates common fail-closed V2 admission (`UNSUPPORTED`), not 24 independent Native bugs.
3. `python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_g4_rig_revision.py tests/moc3/test_studio_g4_meta_cross_redfirst.py --tb=short`: **11 PASS / 11 subtests PASS**.
4. `python -B -m pytest -p no:cacheprovider -q tests/moc3 -rs --tb=short`: **204 PASS / 18 SKIP / 204 subtests PASS**; one visible Studio opt-in and 17 private viewer/dependency/profile skips.
5. `git diff --name-only -- include/ars/ars.h src/abi CMakeLists.txt`: empty; `git diff --check`: PASS with nonblocking Windows LF/CRLF warning.
No GUI input E2E or physical G5 transaction was run, because no G5 writer is enabled and the previous Host desktop input gate was disabled.

## 2. Findings / gap disposition

| ID | Type | Assessment | Required gate |
|---|---|---|---|
| G5-CAS-01 | Hard design blocker | `rig_revision` is a net-state/per-frame G4 read fence, not a commit-per-logical-operation revision. Two opposite edits in one frame can leave `memcmp` equal and hide ABA. | Authoritative `document_revision`, monotonically advance on each committed logical edit/undo/redo, fail closed on overflow. |
| G5-CAS-02 | Hard design blocker | UI and V1 Agent continue writing `s.rig` via different direct paths. IPC writer lock does not coordinate an editor lease or UI draft. | Route all editing through same Native owner-thread transaction authority or explicitly deny legacy edits during leased drafts. No alternate bypass. |
| G5-ACK-03 | Hard design blocker | Client unknown-outcome marker blocks subsequent `AgentBridge.call()`; no G5 outcome lookup on separate read-only recovery path. | Reserved bounded read-only query keyed by session epoch and request ID, not a replay; retain negative marker until evidence reconciles. |
| G5-ID-04 | Contract ambiguity | `draft.*` versus `rig.document.*`; top-level response states `VALIDATED`, `ABORTED`, `APPLIED`, uppercase/lowercase examples differ. | Freeze exact names, mode, fields, state and error codes before source work; keep opt-in RED oracle rather than silently changing implementation to match mock. |
| G5-UNDO-05 | Missing Native proof | Proposed undo/redo modeled only in synthetic `DraftReference`; no owner-thread journal with inverse/forward and redo lineage CAS. | Physical test of new revision on undo and redo, divergent edit clears redo, ABA content equality must not reuse prior revision. |
| G5-FRAME-06 | Missing Native proof | Current render/XPBD pipeline accepts/rejects frame without G5 document epoch/commit identity fence. | On accepted frame only, publish `applied_document_revision`, `solver_revision`, `accepted_frame_generation`. `COMMITTED` ACK cannot mean `APPLIED`. |
| G5-REVIEW-07 | External prerequisite | Independent G4 reviewer and GUI click concurrency evidence remain unavailable. | Separate reviewer + enabled Host input physical E2E before opening G5 writer. |

These are **design/readiness findings**; G5 write behavior is not present, so do not label them proven live G5 writer vulnerabilities.

## 3. Proposed single-owner CAS contract (not yet approved)

### Identity and ownership

- State identity: `session_epoch` (fresh on Studio process/session restart), `model_generation` (fresh on successful model load), `document_revision` (monotonic uint64; independent from G4 `rig_revision`), `draft_id`, `request_id`.
- Separate `applied_document_revision`, `accepted_frame_generation`, `solver_revision`; none inferred from editor revision, IPC ACK or another receipt.
- One Native owner thread owns `EditorDocumentStore`, draft lease, live commit, undo/redo journal and request outcome index. UI, V1 compatibility actions and V2 Agent must pass through the **same validated command path**.
- `document_revision` increases on every successful logical transaction, even if resulting content equals an earlier snapshot. A brush stroke/drag can be one explicitly delimited operation, with samples only in a private draft. Editing and undo cannot advance revision on a failed validation.
- Document revision overflow rejects *before* altering live state. Model reload invalidates drafts and old-session tokens. Scoped G4 revision remains a read-side guard, not a substitute for G5 transaction revision.

### Transaction linearization

1. `draft.begin(session, model, if_revision, owner)`: verify generation, revision, bounded owner identity and availability of a single lease. Return opaque draft ID and base revision, without modifying live document.
2. `draft.edit`: mutate only private copy-on-write candidate; no solver or saved project mutation. `draft.validate` checks full structural, private data and budget invariants; validation is not commit.
3. `draft.commit(session, epoch, draft_id, if_revision, request_id)`: owner-thread atomic step: validate session/lease/candidate/CAS and reserve bounded history+receipt capacity; publish exactly one new immutable document revision + undo entry + request outcome record; release lease; return `COMMITTED` receipt only. If capacity or CAS fails, reject with no partial effect.
4. `physics.apply(if_document_revision)`: latch doc/session/model/solver version, prepare candidate, verify safety/GPU/frame acceptance, then atomically publish `APPLIED` metadata. Failure keeps old accepted frame and editor state remains committed-but-unapplied.
5. `draft.abort`: discard only that owner’s private draft, release lease; never undo an already committed transaction.

No direct mutation of `s.rig` while another owner holds a draft lease. Pending UI edit receives explicit `BUSY`/conflict or a UI-controlled discard/rebase; Agent must not silently steal or invalidate human edits.

### Lost ACK and bounded resolution

- Client timeout/transport ambiguity -> `UNKNOWN_OUTCOME/TIMEOUT_UNKNOWN`, not success/failure; leave quarantine marker, no blind commit/undo/redo replay.
- Owner stores a bounded in-memory immutable outcome receipt keyed `(session_epoch, request_id)` with payload identity and resulting revision **at the same linearization point as commit**, before external ACK. Duplicate ID with different payload is invalid. Same ID after ACK loss is inspected, never reapplied.
- Expose a **dedicated bounded read-only result-status path** that can operate while the ordinary mutation queue is quarantined, without bypassing owner authority or clearing foreign markers. Results: `NOT_SEEN`, `PENDING`, `COMMITTED`, `ABORTED`, `UNKNOWN`, with origin and session guards; `NOT_SEEN` cannot be trusted while a request may still execute.
- Crash/restart rotates session epoch and invalidates volatile receipt lookup. No exactly-once or durable transaction claims across crashes; explicit project save/recover is a separate gate. If outcome cannot be resolved, hold and require safe reconciliation, do not infer `NOT_APPLIED` or retry a mutating action.
- Bound history, request-index, lease lifetime, input size and stale response ownership; full tables reject new mutations instead of pruning unresolved unknown outcomes.

### Undo/redo and ABA

- `undo`: the latest applicable committed transaction, same lineage and current CAS revision, has a validated inverse applied as a NEW committed document revision. Never decrement revision or rewind request history.
- `redo`: latest undo branch only; reject after any divergent committed edit; NEW revision again. Undo/redo are still committed-but-unapplied until a later accepted solver frame.
- Example: content A at revision 7; edit B at 8; undo A at 9; redo B at 10. Request carrying `if_revision=7` is stale even when current content equals A.
- For lost ACK on undo, inspect same request ID and revision before another undo; identical inverse action must never be executed twice.

## 4. Pre-T2 scenario matrix (RED-first/native after gates)

| Scenario | Required oracle | Proof tier |
|---|---|---|
| S01 UI and Agent simultaneous begin | Exactly one lease; loser BUSY; no live mutation | Native + GUI |
| S02 Agent draft open, UI toggle/edit/shortcut | UI cannot bypass lease; explicit conflict | Native + GUI |
| S03 Two inverse edits in one UI frame | Each committed transaction advances doc revision; ABA rejected | Native + GUI |
| S04 Concurrent stale commit / model reload | No partial state, STALE_MODEL or REVISION_CONFLICT | Native |
| S05 Crash/ACK loss before commit | UNKNOWN until bounded reconciliation; no false committed | Native fault injection |
| S06 ACK lost after committed effect | Request-status returns same commit, no double effect | Native fault injection |
| S07 Receipt status while queue quarantined | Safe read-only query works without allowing a new mutation | Native IPC |
| S08 Duplicate request ID, changed payload | Reject collision; no new revision | Native |
| S09 Undo/redo 7→8→9→10, content ABA | New revision each time, old CAS stale | Native + GUI |
| S10 Divergent edit after undo | Redo rejected, prior committed branch unchanged | Native |
| S11 Document commit succeeds, solver/frame rejects | COMMITTED only; applied revision and accepted frame unchanged | Native + frame/GPU |
| S12 Apply after successful accept | APPLIED reports exact applied document/solver/frame generations | Native + frame/GPU |
| S13 Crash/restart and explicit save | Session rotation, no durable unstored claims | Native persistence |
| S14 Private output, malformed IDs and capacity | Denied; no model paths/geometry in public reports | Native security |

## 5. Admission decision

- **G5 contract assessment:** scoped source/design review complete; SPEC REVISE.
- **G4 read fence for G4 bounded reads:** existing GREEN proof retained; NOT promotable by itself to G5 CAS authority.
- **G5 tests-only:** allowed; 11 reference-model synthetic PASS and 24 intentionally RED remain correctly distinguished.
- **G5 T2 Native writer:** **HOLD**. Do not lift until independent G4 review, exact API/schema freeze, common UI/Agent owner-thread edit path, isolated receipt resolution protocol, proper undo journal/frame fence RED-first evidence plan and safe Native GUI acceptance capacity are demonstrated.
- **No merge**, no deployment, no claim of independent sign-off. Public ABI20 remains unchanged.
