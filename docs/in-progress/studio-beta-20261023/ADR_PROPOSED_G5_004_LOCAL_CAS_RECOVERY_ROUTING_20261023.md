# ADR-PROPOSED-G5-004 — Local CAS, ACK Recovery, UI/Agent Routing

**Date:** 2026-10-23
**Version:** 0.1 — PROPOSED / NOT IMPLEMENTED
**Repository:** `boxs-51/Avatar_Runtime`; Windows workspace `beta`, branch `exp/layer-physics-native-studio-local`
**Source baseline:** committed HEAD `2d2fccfaeece6ec686ca09e2e44ae4230574100c` plus uncommitted G4 working tree.
**Review:** SELF_REVIEW_ONLY. Codex independent review unavailable at preparation time.
**Authority:** design specification only; not approval for G5 T2 Native writer, merge or deploy.

## Context, references and design constraints

- `ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md` specifies local-only API V2 and draft operations.
- `ADR_003_RIG_EDITOR_V5_DETAILED_DESIGN.md` describes future editor authority, currently partly using `rig.document.*` terminology.
- `G4_SELF_REVIEW_G5_CAS_BOUNDARY_20261023.md` audits the existing G4 revision and UI/Agent paths.
- `G5_CAS_TRANSACTION_GATE_REVIEW_20261023.md` records 14 pre-T2 Native scenarios and release gates.
- Machine-readable companion: `G5_LOCAL_CAS_ENVELOPE_PROPOSED_20261023.schema.json`.

G4 `rig_revision` is an observation fence based on differences in Native `StudioRig` before/after phases. It is **not** an authoritative, monotonic revision for every successful logical edit. V2 is currently read-only, and its mutation capability must remain **disabled**.

## Decision D1 — One Native editor authority

The Studio owner thread exclusively owns `EditorDocumentStore`, immutable committed document, `document_revision`, private draft/lease, undo/redo journal, bounded receipt index and accepted-frame coordinator.

All Studio GUI interactions, keyboard shortcuts, legacy V1 Agent commands, and future G5 Agent commands must use the same typed `EditCommandBus` **before G5 writes are enabled**. A legacy path that cannot satisfy an active lease must be explicitly rejected or gated off, never bypass the transaction controller by writing directly to `s.rig`.

One opaque session identity per Studio session, new `session_epoch` at restart, new `model_generation` after successful model load. Keep `document_revision` distinct from G4 `rig_revision` and from `applied_document_revision`. The model source is local/private and cannot be embedded in receipts, schemas, logs or Git evidence.

## Decision D2 — Canonical CAS wire contract

For V2 requests, retain the *existing eight-field top-level envelope* with strict JSON decoding:

`api_version`, `request_id`, `session_id`, `expected_model_generation`, `expected_edit_revision`, `operation`, `arguments`, `mode`.

Future edits require a non-null trusted `session_id`, model generation and expected **document** revision. Strictly reject bool-as-integer, duplicate or unknown fields, out-of-budget data, malformed UTF-8, unsupported operations and mode mismatch. Do not grant identity from caller-provided arbitrary `owner` text.

**Canonical operations and modes:**

| Operation | Mode | Meaning |
|---|---|---|
| `editor.state.inspect` | read | Document/applied revisions and status |
| `draft.begin` | stage | CAS acquire one owner lease; private copy only |
| `draft.validate` | stage | Validate all graph/mesh/weight/budget invariants |
| `draft.diff` | read | Bounded local-private draft diff |
| `draft.commit` | edit | Single owner-thread document commit; new revision |
| `draft.abort` | edit | Discard uncommitted draft and release lease |
| `draft.undo`, `draft.redo` | edit | New committed inverse/forward transaction |
| `draft.status` | read | Lookup original mutating request outcome |
| `physics.apply` | edit | APPLIED only after frame acceptance |
| `physics.status` | read | Read accepted applied revision and frame fence |

The `rig.document.*` operations in ADR-003 are **design aliases, not callable V2 operations**. The companion JSON Schema freezes the proposed envelope and arguments for these top-level operations; per-element `draft.edit` must be specified and reviewed separately. Do not silently implement aliases.

Response fields are `api_version`, `request_id`, `state`, `result`, `error`. Canonical uppercase states:
`COMPLETED_READ`, `STAGED`, `VALIDATED`, `COMMITTED`, `ABORTED`, `APPLIED`, `REJECTED`, `UNKNOWN_OUTCOME`.
Rejection and unknown outcome carry `result=null`; successful states have `error=null`.

## Decision D3 — Draft/commit linearization

`draft.begin` validates session/model/current document revision, reserves one bounded lease and creates a private candidate. The live document, model asset, solver and accepted frame remain unchanged.

`draft.validate` rechecks integrity of all edited relationships and budgets. `draft.commit` on the Native owner thread reserves journal and receipt capacity **before** publication; validates session, lease holder, draft, model and CAS revision; publishes exactly one new document revision, immutable receipt and undo entry as one in-process linearization step. If capacity, validation or CAS fails, no partial effect is published.

Every successful logical transaction advances `document_revision` even if the resulting document bytes equal a prior state. Overflow must fail closed. A multi-sample gesture is an explicitly delimited draft followed by one logical commit, not arbitrary direct UI mutations.

`COMMITTED` means in-memory editor-document publication, **not** saved to disk, physics-applied, GPU-completed or displayed. `APPLIED` requires an exact `document_revision`, solver/frame epoch and accepted-frame evidence; failed acceptance retains last applied version and frame.

## Decision D4 — Lost ACK / UNKNOWN_OUTCOME recovery

`scripts/studio_agent_api.py::AgentBridge.call` presently retains `uncertain_outcome` after uncertain ACK and blocks later commands on the ordinary queue. A recovery request through the **same** API is therefore insufficient.

Introduce a **separate strictly read-only bounded recovery IPC lane**, operated by the same Native owner and accessible while mutation transport is quarantined. The only supported logical request is `draft.status`, with its own request ID and `arguments.request_id` referring to the original mutation. Require originating session and epoch: request ID alone must not expose private receipts.

At the commit linearization point, record an immutable, bounded `ReceiptIndex[(session_epoch, original_request_id)]` holding original operation/payload identity, terminal outcome and document revision before external ACK. Duplicate request ID with a different payload is rejected; same-ID retry never re-applies mutation. If receipt capacity is exhausted, reject *before* commit rather than evict unresolved entries.

Recovery outcome states: `PENDING`, `COMMITTED`, `REJECTED`, `ABORTED`, `UNKNOWN`. Absence of a receipt is **UNKNOWN**, not proof that no effect occurred. A client with lost ACK returns `UNKNOWN_OUTCOME/TIMEOUT_UNKNOWN` and cannot blindly retry `commit`, `undo` or `redo`; it may only query recovery status. After an authenticated terminal receipt is reconciled, a bounded safe protocol may clear the matching quarantine marker. Crash/restart rotates epoch and invalidates volatile receipts: **no durable exactly-once claim** without a separate persistence gate.

Do not allow the recovery lane to mutate, consume unknown foreign markers, enumerate model data or arbitrary filesystem locations.

## Decision D5 — Undo, redo and ABA

Undo verifies current revision/session and eligible journal lineage; it publishes the inverse as a **new** committed revision. Redo only replays the latest non-diverged redo branch as a **new** revision. Any divergent commit invalidates redo. Example: content A@7 → B@8 → undo A@9 → redo B@10. A request with `expected_edit_revision=7` remains stale even when the content currently equals A.

Lost ACK during undo/redo follows exactly the same immutable receipt/recovery rule. A second undo cannot be issued while the previous outcome remains uncertain. Accepted frame state is updated only by a separate successful apply; undo/redo do not roll back a previously accepted GPU frame automatically.

## Admission matrix (must be RED-first, Native where applicable)

| Case | Requirement and oracle |
|---|---|
| CAS-01 | Simultaneous GUI and Agent begin: exactly one lease; loser BUSY |
| CAS-02 | Keyboard/GUI/V1 edits cannot bypass active G5 lease |
| CAS-03 | Edit→undo in same frame gives distinct document revisions despite ABA |
| CAS-04 | Model reload or stale revision rejects without any partial edit |
| ACK-05 | ACK loss before commit stays UNKNOWN until safe terminal evidence |
| ACK-06 | ACK loss after commit reconciles exactly one revision, no duplicate effect |
| ACK-07 | Recovery remains available with ordinary queue quarantined |
| ACK-08 | Duplicate ID with different payload rejected |
| UNDO-09 | Undo/redo 7→8→9→10, old CAS rejected |
| UNDO-10 | Divergent edit invalidates redo |
| FRAME-11 | COMMITTED without accepted frame never claims APPLIED |
| FRAME-12 | Accepted frame uses matching doc/solver/frame fences |
| SEC-13 | Invalid IDs/paths, overflow, privacy leak and unauthorized caller denied |
| LIFE-14 | Crash/restart invalidates volatile tokens; no unproven durability |

## Security and privacy boundaries

Studio is Local-only and Computer Bridge presently reports `USER_CONTEXT_NO_OS_SANDBOX`; do not treat that as process isolation. IPC token prevents accidental response mismatch but is not independent OS identity authorization. Input parsers, owner/session checks, local storage ACLs, no arbitrary user-selected output paths and bounded receipts are necessary; they still do not establish isolation against an adversarial same-user process.

Raw/private `.moc3`, model JSON, textures, geometry arrays, embedded payloads, screenshots and original model paths must stay outside Git. E2E uses private local source from `_private_manifest`; exports are limited to categorical PASS/RED/BLOCKED summaries. Any newly introduced local logs belong under ignored `build/studio-user/` or `local-assets/`.

### Clarification: non-public model information, not local model access

Per [ADR-005](../../reference/ADR_005_MODEL_INFORMATION_NONPUBLIC_AND_SDK_PACKAGING.md),
private model information must **not be published** in the repository, releases,
SDKs, logs, screenshots, reports or GitHub evidence. Local authorized Studio/UI
and Agent inspection/editing remains governed by the existing local runtime
contract; model confidentiality alone is **not** a new deny-read permission
layer. G5 receipts must contain only bounded transaction metadata and a
canonical operation fingerprint, never raw geometry, paths or model payloads.
The G5 writer stays HOLD for the separate CAS/recovery/concurrency gates.

## Status and rollback

**Status:** PROPOSED, not owner-approved runtime contract. G5 Native writer = **HOLD**. Independent G4 review and GUI Agent concurrency remain pending. Current V2 read-only reject policy stays unchanged. No public SDK ABI20 modifications.

Rollout sequence after separate approval: frozen schema → true Native RED-first tests with disposable fixture → owner-thread controller behind explicit opt-in → UI/V1/V2 migration with bypass denial → fault-injected lost-ACK, undo/redo, frame-acceptance and privacy qualification → independent review. Failure restores disabled writer flag without altering G4 readback.

No source changes, merge, deployment or automatic activation are authorized by this ADR.
