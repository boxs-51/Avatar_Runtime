# G4 Local-only UTF-8 model ID transport — implementation checkpoint

Date: 2026-10-23
Branch: exp/layer-physics-native-studio-local
Starting HEAD: fcb6eb907e24245ff1a454cbde2d01bd018786a4
Role: engineering self-test; independent audit NOT_RUN.
Release disposition: HOLD. G4 remains PARTIAL.

## Purpose and bounds

The older G4 Native Agent ID readback used whitespace-sensitive
ASCII tokens (1..59 characters). Unicode names with spaces could
not be enumerated or looked up. This checkpoint replaces the raw
wire ID field with a bounded lowercase hexadecimal encoding of
the UTF-8 bytes. All bytes travel as one ASCII IPC token.

Native read-only generation-fenced operations:
- PARAM_ID_AT / PART_ID_AT: return index + idhex (not raw ID).
- PARAM_FIND_AT / PART_FIND_AT: accept an ID represented by hex bytes.
- Exact reverse lookup refuses unknown/duplicate matches.
- The Native ID buffer supports 1..96 bytes, hex wire at most 192
  characters. Oversized or malformed inputs fail closed.
- Python V2 decodes idhex with strict UTF-8; malformed sequences,
  control chars, slash, invalid scalars or oversized names fail closed.
- IDs containing ordinary spaces and UTF-8 letters can now be
  represented without changing the public SDK ABI20.

Security scope: trusted local AgentBridge only. Authorization to read
LOCAL_PRIVATE model data remains explicit; ID bytes are not filenames
or path tokens. No network endpoint or model export is added.

## Evidence

G4-UTF8-01 BASELINE_RED:
A six-case synthetic suite on the prior ASCII-only implementation
produced two semantic failures (UTF-8 page and exact lookup) and
one UTF-8 wire validation error; a separate invalid-scalar fixture
could not serialize its surrogate with ensure_ascii=false. This
fixture-encoding error was classified as HARNESS_ERROR, not a
Native runtime defect. Test source now uses ASCII Unicode escapes,
preserving the UTF-8 runtime values.

G4-UTF8-02 POSTFIX_UNIT:
Six cases PASS for UTF-8 wire encoding, Vietnamese/Japanese paged
readback, reverse lookup, invalid decoded sequence rejection,
size/control restrictions and unbound access rejection.

G4-UTF8-03 BACKWARD_COMPATIBILITY:
Existing V1 Agent API, V2 readback, generation, F02 client UNKNOWN_OUTCOME
regressions PASS after updating only the expected ID wire contract.

G4-UTF8-04 NATIVE_WINDOWS:
Studio RelWithDebInfo and Release builds PASS. Physical local-model
E2E page/ID lookup, Native grammar, Agent API, IPC race and process
restart PASS. Native grammar matrix: 64/64 malformed inputs rejected,
including six additional hex-format cases at a valid model generation.

## Unverified

Physical runtime readback of a model containing genuinely non-ASCII
MOC3 IDs remains NOT_RUN because there is no independently verified
nonprivate fixture with such IDs. Synthetic Unicode unit tests and
real Windows builds do not replace that specific physical proof.
UTF-8 over 96 bytes, controls and slash are intentionally denied.
No frame-atomic value snapshot, revision-fenced editing, or V5
mutation has been introduced.

The separately maintained DEFERRED_GATES_AND_G4_SCOPE_20261023.md
continues to hold unverified owner-crash, power-loss and
security-review cases. F02 and G4 remain PARTIAL, F03 HOLD,
F06 OPEN, Studio RELEASE HOLD.
