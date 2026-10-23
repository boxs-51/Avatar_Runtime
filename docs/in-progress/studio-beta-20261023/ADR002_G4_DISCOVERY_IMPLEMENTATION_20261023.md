# ADR-002 G4 — Agent Toolkit V2 discovery checkpoint

- Date: 2026-10-23
- Branch: exp/layer-physics-native-studio-local
- Base: e85977e70aabe1becb58b39d8e0441ab5593c8e1
- Authority: development self-test only, not independent audit
- Result: SCOPED GREEN for offline discovery; release HOLD

## Implementation

scripts/avatar_toolkit_v2.py provides a standalone strict JSON-byte decoder.
scripts/avatar_toolkit_v2_cli.py provides capabilities, schema and bounded stdin calls.
Only avatar.capabilities and avatar.schema return COMPLETED_READ. Native V2,
model access, editing tools and Computer Bridge H5 Avatar registration are disabled.

Required envelope fields: api_version, request_id, session_id,
expected_model_generation, expected_edit_revision, operation, arguments, mode.
Session, model-generation and edit-revision values must remain null. Only mode
read is enabled. Max request is 65536 bytes. Duplicate keys, invalid UTF-8,
unknown fields, wrong version, nonfinite scalars, integer overflow and extra
tokens fail closed. Errors return generic codes without echoing bad payloads.

## Finding monitoring

- F01: PROVISIONAL_GREEN, Native 48-case grammar and valid API regressions pass; independent review pending.
- F02: PARTIAL. Reproduced a truncated own ACK (token OK with no completed frame) falsely succeeding. Fixed client receipt requirement: full newline-terminated frame, typed status and nonempty receipt. Stale foreign ACK is not removed; native direct-writer, crash journal and recovery remain open.
- F03: HOLD. Private unreachable local Git objects preserved; no GC, prune or history rewrite.
- F04: PARTIAL. V2 outputs known static public-safe catalog fields, but general report promotion still needs enforcement.
- F05: PROVISIONAL_GREEN, durable rig tests PASS; power-loss, antivirus sharing and forced replacement denial remain unproven.
- F06: OPEN. Native controller and dispatcher extraction outstanding.
- F07: PARTIAL. 256 deterministic JSON mutations and isolated CLI tests pass; not Native fuzz or GUI stress certification.

## Deliberately unimplemented

No named pipe transport, Native V2 command dispatch, active model sessions,
session epoch, generation/revision CAS, request journal, editor lease, commit,
undo, remote service, arbitrary file paths, H5 tool-group, bone DAG or V5 skinning.
Legacy Studio V1 remains the model control route. The new module is not an
authorization layer, same-user isolation or a guarantee of exactly-once mutation.

## Next gates

G2: Native owner/journal/UNKNOWN_OUTCOME and restart RED-first matrix.
G3: Mandatory privacy-safe artifact boundary and durability fault matrix.
G4: Read-only native session and snapshots only after G2/G3 pass.
G5: Staged CAS editor commands only after G4 is independently reviewed.
Release remains HOLD; public SDK ABI20 remains unchanged.

Raw test logs and model information remain on the Windows Host in ignored
build/studio-user; no model data, screenshot or receipt is embedded here.
