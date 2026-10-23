# G5 V2 Editing - T0/T1 RED-first (2026-10-23)

Baseline: beta@2d2fccfaeece6ec686ca09e2e44ae4230574100c, dirty local working tree.
G4 handoff: G4_HANDOFF_G5_ENTRY_20261023.md. Independent G4 review NOT_RUN; GUI input BLOCKED.
Runtime G5 mutations: HOLD. T1 is TESTS-ONLY; not a source implementation or approved interface.

Source: ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md and ADR-003, both PROPOSED.
Provisional design: single-owner local Studio, explicit session+generation+document revision;
CAS draft.begin, private draft validate and diff, commit separate from accepted XPBD frame,
undo/redo increasing revision, reject concurrent UI/Agent writes, fail-closed unsafe retry.
No arbitrary paths, network, public SDK ABI change or private model export.

RED-FIRST T1 executable: tests/moc3/g5_redfirst_contract.py
Command: python -m pytest -q tests/moc3/g5_redfirst_contract.py --tb=short
G5-C01 capabilities still advertise edits disabled: CONTROL PASS.
G5-C02 unauthorized draft is rejected without Native call: CONTROL PASS.
G5-R01 exact CAS draft.begin expects STAGED and draft_id: RED unsupported.
G5-R02 mismatched revision expects REVISION_CONFLICT: RED unsupported.
G5-R03 commit without validated draft expects INVALID_DRAFT: RED unsupported.
G5-R04 stale-revision undo expects REVISION_CONFLICT: RED unsupported.
Observed: 2 PASS, 4 expected RED. V2 currently refuses non-null session/edit revision
and mode other than read. This is correct CURRENT fail-closed behavior, not a defect
to bypass. No actual writer was enabled.

Future T2 physical matrix (NOT_RUN):
single-writer UI/Agent conflict, stale model reload, invalid DAG/weights,
aborted-draft no-effect, post-commit readback, ACK lost after effect and
UNKNOWN_OUTCOME reconciliation, monotonic undo/redo under ABA,
solver/frame acceptance distinct from document commit, durability and privacy.
Independent G4 acceptance plus approved G5 schema are prerequisites before
Native CAS mutation integration. Release HOLD; do not merge.

## Expanded RED-first checkpoint (same day)
See G5_V2_EDITING_EXTENDED_RED_FIRST_20261023.md. Opt-in 24 intentional RED, 6 control PASS, and 11 reference-model synthetic PASS; V2 Native writes remain disabled, G5 T2 HOLD.
