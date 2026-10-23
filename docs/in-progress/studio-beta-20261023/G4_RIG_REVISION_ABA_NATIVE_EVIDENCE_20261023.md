# G4 Rig Revision — Native ABA and Regression Checkpoint (2026-10-23)

Workspace: Windows beta, exp/layer-physics-native-studio-local
HEAD: 2d2fccfaeece6ec686ca09e2e44ae4230574100c
Patch state: uncommitted, no merge, push or deployment.

## Contract reconciliation
The old synthetic test expected editor_revision_available=false despite
the new Native G4_RIG_REV opcode and version-fenced Rig V4 readback.
Updated tests/moc3/test_studio_g4_mesh_rig_read.py to assert both
editor_revision_available=true and editor_revision=1 on its valid fixture.
Preserved its actual rig-value and negative-input assertions. Created a
pre-edit backup in ignored build/studio-user/g4-rig-revision-backup-20261023.

## Physical Native ABA — validated
Harness: python scripts/test_studio_g4_ui_race_live.py --run-aba
Two checked physical runs: PASS (zero exit) with
G4_NATIVE_ABA=REJECTED_ABA_STALE_REVISION.
The owner-local Studio process starts with two rig anchors [0,1].
A paused Agent V2 read receives its first native item; another Native
Agent adds anchor 2, checks intermediate [0,1,2] and increasing revision,
then removes anchor 2, checks final [0,1], and verifies a further revision
increase. The paused read must return REJECTED/STALE_REVISION with no page.
Native case JSON confirms actor=AGENT_IPC_ABA, edit_verified=true,
revision_increased=true, error_code=STALE_REVISION. No private raw model,
coordinates or file identifiers are placed in public evidence.

Non-ABA control: G4_NATIVE_AGENT_RACE=REJECTED_AFTER_EDITOR_CHANGE PASS.
Uncontended physical G4: 37/37 PASS in two Studio processes.
Desktop input was previously DENIED by Host; this proves Agent/Agent
Native concurrency, not GUI click-driven edits. GUI E2E remains BLOCKED.

## Regression
- G4 focused: 9 PASS, 13 subtests PASS
- Extended Python suite: first attempt 56 PASS / 1 FAIL due to F02
  test_complete_ack_releases_quarantine timing; the synthetic ACK thread
  failed to observe request.txt within its one-second deadline.
- Same F02 test isolated: 1 PASS
- Full extended suite repeated twice: 57 PASS, 85 subtests PASS each time
- Windows Release CTest: 10/10 PASS, including ABI20 C11/C++20
- git diff --check: PASS (nonfatal line-ending warnings only)
- Public ABI20 source paths unchanged

Record the initial F02 failure as an intermittent test/timing finding,
not a regression proven to be caused by G4, and do not weaken F02
security oracles. Re-run under increased CI/host contention before
classifying F02 as stable.

## Evidence identity (SHA-256 prefixes, local working tree)
- scripts/test_studio_g4_ui_race_live.py: 49c5ac9d9eef68da9892
- tests/moc3/test_studio_g4_mesh_rig_read.py: 109460a5078b7d3e1a68
- tools/avatar-studio/studio.c: 6a90594b5872489fa6f8
- scripts/avatar_toolkit_v2_local.py: d5c4828b20b4329d1c4e

Remaining gates: independent code/security review NOT_RUN, true GUI
concurrency BLOCKED, model hot-reload/editor frame acceptance outside
scope. Release HOLD, no merge.
