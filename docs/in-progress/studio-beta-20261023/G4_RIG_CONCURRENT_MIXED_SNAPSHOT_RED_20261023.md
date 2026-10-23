# G4 Rig V4 mixed-snapshot — RED-first Native E2E (2026-10-23)

Workspace: local Windows beta, branch exp/layer-physics-native-studio-local.
Git HEAD: 2d2fccfaeece6ec686ca09e2e44ae4230574100c.
Working-tree G4 code from earlier steps remains uncommitted. No merge/push/deploy.

## Scope and actual authority

The user requested physical UI/Agent concurrent rig edit versus G4 readback,
RED-first. Native test process and native Agent IPC are authorized in a
disposable ignored build folder. However, Computer Bridge desktop.action
returned DESKTOP_INPUT_DISABLED on a focus attempt targeting the dedicated
Studio PID/window. Respect that host gate: GUI click, type and actual
UI-triggered edit E2E were NOT_RUN / BLOCKED; no Win32 injection or terminal
desktop-input bypass was attempted.

## RED-first scenario matrix

| ID | Actor / condition | Expected oracle | Observation |
|---|---|---|---|
| G4-RACE-00 | Native Studio starts with local model, selected rig | Consistent starting rig | PASS, two anchor entries [0,1] |
| G4-RACE-01 | G4 reads first anchor, pauses before second | Can interleave edits | PASS, deterministic barrier |
| G4-RACE-02 | Other Native Agent IPC removes 0, adds 2 | Rig now [1,2] with same count | PASS, direct Native entry readback |
| G4-RACE-03 | Resume G4 rig.entries.page after mutation | Must not return a mixed accepted page | **RED:** COMPLETED_READ with anchors [0,2] |
| G4-RACE-04 | No concurrent mutation | Healthy page remains accepted | PASS, existing G4 E2E/regression controls |
| G4-RACE-05 | GUI click changes rig during read | Actual UI/Agent concurrency | BLOCKED, DESKTOP_INPUT_DISABLED |
| G4-RACE-06 | Native process cleanup, no original file write | Isolated fixture, no publish | PASS, bounded child termination |
| G4-RACE-07 | Stale model and malformed G4 commands | Fail closed | PASS, earlier 37-case Native physical E2E |

## Reproducer

`python scripts/test_studio_g4_ui_race_live.py --run-agent`

The program runs two Agent roles against a real Windows Native Studio
binary: one uses the trusted-local V2 read adapter and pauses at the
second rig.entry read; the second performs the two sanctioned Native rig
edits without save or solver apply. It verifies pre/post counts and entries.
Expected exit code 2 with categorical status
`G4_NATIVE_AGENT_RACE=SEMANTIC_RED_MIXED_SNAPSHOT` indicates the
**reproduced defect**, not a harness/import/infrastructure error.
Two consecutive reproductions of this RED condition were observed.
A stable no-edit test suite remains GREEN (43 Python tests, 82 subtests).
No fixture ID, model path, per-vertex position, screenshot or raw IPC
receipt is exported in the report; private logs stay under ignored
`build/studio-user`.

For later GUI E2E after Owner enables H5 desktop input, use the
`--begin` staged mode only on a fresh dedicated Studio test window,
verify the PID/geometry, click UI to replace anchors [0,1] with [1,2],
and then use `--finish <case_dir>`. A GUI-based PASS must have both
H5 action receipts and the Native post-edit oracle. Agent-only RED is
not evidence that H5 GUI controls work.

## Root cause / next fix boundary

`scripts/avatar_toolkit_v2_local.py::read_geometry` checks only model
generation and aggregate model counts after individual Native rig
entry reads. The rig is mutable on the Native owner thread; both UI
and Native Agent writes can update it without changing generation or
model counts. G4 has no editor-revision fence, so distinct rig states
may be combined in one page. A future fix must add a true monotonic
per-edit revision covering UI and Agent rig mutations, fence every
Native entry and metadata read or perform an atomic on-owner-thread
bounded snapshot, then rerun the unchanged RED oracle to GREEN.
Do not claim a fingerprint-only check catches ABA.

Finding: G4-RIG-MIX-001, actionable correctness RED.
G4 readiness: REVISE / release HOLD. GUI-path proof: BLOCKED.
Independent reviewer: NOT_RUN. Public SDK ABI20: unchanged.
