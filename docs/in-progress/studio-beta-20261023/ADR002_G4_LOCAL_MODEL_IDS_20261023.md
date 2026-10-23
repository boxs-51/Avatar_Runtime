# ADR-002 G4 — Local parameter/part ID discovery and exact lookup

- Date: 2026-10-23
- Branch: exp/layer-physics-native-studio-local
- Starting source: 95a90ce (F02 Windows sharing harness)
- Authority: engineering self-test; independent auditor approval absent
- Decision: G4 PARTIAL GREEN / RELEASE HOLD

## Local-only implementation

Native Studio now handles read-only, model-generation-guarded operations
PARAM_ID_AT, PART_ID_AT, PARAM_FIND_AT and PART_FIND_AT. Enumeration
returns one model-local ID for an index. Lookup scans the current Native
model and returns exactly one matching index; duplicates, unknown IDs,
stale generations and invalid grammar fail closed. Parameter/part IDs are
treated as LOCAL_PRIVATE values, never as filenames or capabilities.

Existing V1 Python AgentBridge validates every new command before writing
IPC. The V2 trusted-local adapter adds IDs to four-item parameter/part
pages and exposes catalog.parameter.lookup and catalog.part.lookup.
The caller must explicitly bind a local bridge and allow LOCAL_PRIVATE
readback; optional expected_model_generation ensures consistency with
a previously observed model generation. All reads include native epoch
checks; lookup also checks ID/index readback and final model epoch/counts.

## Deliberate coverage limit

This bounded first slice accepts ASCII model identifiers containing
letters, digits, underscore, hyphen, dot or colon, length 1–59 bytes.
Other model IDs are rejected without echoing raw bytes. Complete
arbitrary UTF-8 coverage is NOT claimed. No private ID list is placed
in this report. There is still no frame-atomic snapshot, editor-revision
CAS, model lifecycle API, mutation, undo/redo or V5 bone/skin write path.

## RED-first and GREEN evidence

- Synthetic RED reproduced absent V2 ID pages/lookup.
- Synthetic GREEN: parameter and part ID pages, exact lookup,
  duplicate/unknown/unsafe-ID rejection before Native access,
  and stale generation refusal.
- Native RelWithDebInfo + Release builds PASS.
- Native grammar expanded to 58 malformed command cases: PASS.
- Real local private-model E2E: generation-fenced parameter/part
  pagination and reverse ID lookup PASS, without printing names.
- V1 Agent contract, V2 regression, public-safe reporter, XPBD
  contract, Native agent E2E, restart, IPC race, rig save all PASS.

One aggregate execution failed because its log output path included
a wildcard * (invalid Windows filename), not because a V2 assertion
failed. Re-running V2 using a fixed filename PASS. This harness path
error is retained separately from production regression outcomes.

## Findings and security

F01 PROVISIONAL GREEN; F02 PARTIAL; F03 HOLD (local Git-object custody);
F04 PARTIAL; F05 PROVISIONAL GREEN; F06 OPEN; F07 PARTIAL.
Native IPC same-user trust boundaries remain unchanged. Do not include
private model IDs or raw numeric receipts in Git, reports, CI or issues.

Logs and private E2E evidence remain in Git-ignored build/studio-user.
No GitHub push, merge, object prune or deploy.

Next: complete UTF-8 ID coverage without truncation or ambiguity;
owner model lifecycle/read snapshot constraints, editor revision and
accepted-frame fences, then V5 rig transactions after security gates.
