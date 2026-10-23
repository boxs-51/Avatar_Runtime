# G4-REV-01 / G4-VAL-02 — RED-first remediation (2026-10-23)

## Exact state
Repository boxs-51/Avatar_Runtime; Windows workspace beta, branch exp/layer-physics-native-studio-local, committed HEAD 2d2fccfaeece6ec686ca09e2e44ae4230574100c. Existing dirty G4 working tree preserved. No commit, push, merge or deploy.

## Findings
G4-REV-01, scripts/avatar_toolkit_v2.py: avatar.schema previously returned editor_revision_fence=false for all reads; fixed to true for rig.local.inspect and rig.entries.page only. Mesh operations remain false.

G4-VAL-02, scripts/avatar_toolkit_v2_local.py: the source rig's vertex bound is now enforced for parent.child as it already was for parent.parent. Out-of-bounds Native receipts return REJECTED/INVALID_DRAFT with result=null. The cross-link child belongs to a different target-drawable domain and was not altered.

## Baseline RED / post-fix GREEN
Oracle: tests/moc3/test_studio_g4_schema_bounds.py, unchanged after initial RED.
Baseline command: python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_g4_schema_bounds.py --tb=short
Baseline: four failing subcases — both Rig schema operations, child=3 and child=999; valid and mesh controls passed.
After patch same command: four tests and five subtests PASS.
Extended regression: 61 Python tests and 90 subtests PASS.

## Native Windows physical E2E
python -B scripts/test_studio_g4_native_live.py => 37/37 PASS, two independent sessions.
python -B scripts/test_studio_g4_ui_race_live.py --run-aba => REJECTED_ABA_STALE_REVISION PASS.
python -B scripts/test_studio_g4_ui_race_live.py --run-agent => REJECTED_AFTER_EDITOR_CHANGE PASS.
python -B scripts/test_studio_g4_parent_live.py => 8/8 PASS. Physical test covers private in-memory parent add, parent/child readback, rejection of out-of-range Native write and invalid read index, valid readback after negative command, and schema. Model remains unsaved.

Invalid Native receipt injection was synthetic at the Python adapter; a corrupt Native model was not constructed. Native C handler and executable were unchanged in this task; E2E uses freshly modified Python read adapter against previously built Native Studio.

## Compatibility and follow-up
Release CTest 10/10 PASS including prep_abi20_c11/cpp20; ABI20 public paths unchanged. git diff --check PASS with line ending warnings only. Pre-fix backups in ignored build/studio-user/g4-rev01-val02-before-fix-20261023.

Independent reviewer NOT_RUN, real GUI concurrent input BLOCKED per earlier Host result. G5 T2 Native writer stays HOLD pending separate acceptance. No merge/deploy and no source changes outside two narrow fixes.
