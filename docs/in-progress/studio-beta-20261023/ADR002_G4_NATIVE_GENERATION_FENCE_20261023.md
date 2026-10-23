# ADR-002 G4 — Native model-generation fence (Local-only)

- Date: 2026-10-23
- Branch: exp/layer-physics-native-studio-local
- Starting HEAD: 7c0c424b18fa664ef6b0c0c16d1dc77c03c884a0
- Role: local engineering; no independent auditor approval
- Status: G4 READ GATE PARTIAL GREEN; RELEASE HOLD

## Design and ownership

The Native Studio process exclusively owns the loaded model and assigns
a new nonzero, Windows-CNG-generated, 63-bit opaque generation on every
successful model load. The new generation is generated before replacing
the previous valid model. CNG failure preserves the old loaded model
and releases the candidate resources. Generation is session-local and
probabilistically unique across restarts; it is NOT a model hash, path,
identity proof or monotonic edit revision.

Native read-only opcodes:
- STUDIO_EPOCH -> generation=<unsigned decimal>
- PARAM_INSPECT_AT <generation> <index>
- PART_INSPECT_AT <generation> <index>

The two guarded inspect handlers check generation and index on the
model-owning UI/dispatcher thread before returning data. A wrong
generation yields ERR and does not publish model fields. Legacy v1
inspect commands remain unchanged for compatibility.

The trusted-local V2 adapter reads epoch before/after status and uses
generation-bound Native inspect for every paged item. Each page also
checks epoch and aggregate counts after reading, failing closed on
STALE_MODEL. Unlike prior count-only guards, it detects model swaps
with equal item counts and process restarts with a new CNG generation.
The V2 envelope permits optional positive expected_model_generation
for these LOCAL_PRIVATE reads only. Wrong values fail with STALE_MODEL.
Offline discovery, write operations and requests with session_id or
expected_edit_revision are not granted new capabilities.

## RED-first evidence and outcomes

G4-EPOCH-01: absent model epoch / unguarded inspect calls ->
synthetic RED, then GREEN.
G4-EPOCH-02: same-count model switch during page ->
synthetic RED, then STALE_MODEL GREEN.
G4-EPOCH-03: missing native epoch / malformed generation ->
fail closed GREEN.
G4-EPOCH-04: expected_model_generation matches / mismatches ->
synthetic RED, then GREEN.
G4-EPOCH-05: real Windows Native epoch, guarded inspect, stale
generation and correct V2 CAS -> physical GREEN without exposing the
generation in printed outputs.
G4-EPOCH-06: Release Studio built fresh and private-model readback
E2E GREEN. Existing V1 Native grammar, API, IPC race, rig, XPBD and V2
contract regressions GREEN in this development pass.

Detailed stdout, model fixture and receipts are retained only in the
Git-ignored build/studio-user/ directory. No screenshots, private
identifiers, coordinates, model-derived counts or raw receipts are
included here.

## Still open

Editor-document revision/CAS, UI mutation lineage, accepted frame
generation, model-lifecycle API, ID lookup, mesh/rig readback, V5
transaction/undo and complete UI parity are NOT implemented.
One page is NOT an atomic editor-state snapshot: parameter values may
update during animation without changing model generation. Native
guarding prevents cross-model mixing, not cross-frame variation.
CNG generation uniqueness is probabilistic and cannot serve as
same-user process authentication. Do not equate an IPC OK with an
accepted XPBD frame.

Findings remain F01 PROVISIONAL GREEN, F02 PARTIAL, F03 HOLD,
F04 PARTIAL, F05 PROVISIONAL GREEN, F06 OPEN, F07 PARTIAL.
G4 PARTIAL; product RELEASE HOLD. SDK ABI20 unchanged.

## Additional restart oracle

G4-EPOCH-08: launch two sequential Windows Native Studio processes
against the same owner-approved private local fixture. Both sessions
successfully read a native generation; the second session receives
a different opaque generation and rejects a guarded request carrying
the previous process generation. This physical check PASS is evidence
of process-restart freshness on the tested Host, not a statistical
proof against all random-token collisions or a durable request journal.
Only categorical PASS is printed; full local receipts remain ignored.
