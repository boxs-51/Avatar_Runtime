# G4-VAL-03 / G4-SCHEMA-04 RED-first repair — 2026-10-23

## Provenance and boundaries
- Repo boxs-51/Avatar_Runtime; Windows Computer Bridge workspace alias beta; branch exp/layer-physics-native-studio-local.
- Exact committed HEAD: 2d2fccfaeece6ec686ca09e2e44ae4230574100c. Working tree remains dirty; SHA-256 of changed sources below pins tested code.
- User requested RED-first repair for self-parent, cross target/child validation, accurate schema, regression and ABI20. No merge or release.
- Local pre-edit verified backups: build/studio-user/g4-val03-schema04-before-fix-20261023 (two source files, one synthetic fixture).
- No Native C writer, public ABI, network service, model export or unrequested source was changed.

## Findings and bounded patches
- G4-VAL-03 / scripts/avatar_toolkit_v2_local.py:
  - Reject parent.child == parent.parent as INVALID_DRAFT.
  - Reject cross.target == source drawable, or >= current drawable count.
  - Read G4 mesh metadata for the *target drawable* using existing generation-fenced, read-only g4.mesh.meta; reject cross.child >= target vertices.
  - Reject unavailable target metadata. If receipt claims another drawable, fail closed as STALE_MODEL (not INVALID_DRAFT).
  - Check rig revision again after reads. Do not incorrectly bound cross.child by the source rig vertex count.
- G4-SCHEMA-04 / scripts/avatar_toolkit_v2.py:
  - mesh.vertices.page / mesh.triangles.page expose only drawable/start/limit.
  - rig.entries.page alone exposes kind with valid enum; rig.local.inspect and topology keep drawable-only.
- Old synthetic fixture test_studio_g4_mesh_rig_read.py receives a valid target metadata reply; existing behavioral assertions unchanged.

## RED-first and GREEN commands/evidence
- Targeted new test: python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_g4_relations_redfirst.py --tb=short
  - PRE-FIX: 10 failed subcases and 5 controls PASS. Faults were semantic, not harness import errors.
  - POST-FIX: 8 tests PASS, 7 subtests PASS; spoofed target receipt strictly requires STALE_MODEL, while unavailable target requires INVALID_DRAFT.
  - Early post-fix single test discrepancy (STALE_MODEL vs INVALID_DRAFT) led to a *more precise error classification assertion*, not weakening fail-closed.
- Affected Python suite: 69 tests PASS, 97 subtests PASS.
- Full default tests/moc3: 197 PASS, 18 SKIP, 196 subtests PASS, repeated successfully.
  - 1 SKIP: visible Studio test requires opt-in.
  - 17 SKIP: private viewer/dependencies/profile unavailable. Not claimed as tested.
- Native Windows live G4 E2E: 37/37 PASS, two Studio processes.
- Native parent E2E: 8/8 PASS.
- Native ABA: REJECTED_ABA_STALE_REVISION PASS.
- New Native cross E2E: scripts/test_studio_g4_cross_live.py => 5/5 PASS, including child in target beyond source's index range and invalid target child rejected with no revision increase. Existing owner-local model used privately; no save/apply or model export.
- Release CTest: 10/10 PASS, including ABI20 C11 and C++20.
- git diff --check: exit 0 (Windows LF/CRLF advisory only).
- git diff --name-only -- include/ars/ars.h src/abi CMakeLists.txt: empty.

## Current SHA-256 values
- scripts/avatar_toolkit_v2.py: 0af581c5dcd074f9eebcfdf4ba3c81a9795569e3bac813bc7138c3b193b9b21a
- scripts/avatar_toolkit_v2_local.py: f9a924b6db1c25fd965b11025188316b69dc88e3d28945bcf84b3edfa1165fdf
- tests/moc3/test_studio_g4_mesh_rig_read.py: 82bc85205200f78cd1d650ed692e28ba214415666c60c84b205569bbf60b2f3d
- tests/moc3/test_studio_g4_relations_redfirst.py: fb118fa70ba7f6437e3ce715daf472f62e28d477543c6fc7601ceb6885d8967a
- scripts/test_studio_g4_cross_live.py: 5e0a06f0fd4a20e7db71295a4904d42e6d9c2cd89198722bf3fbfaa5feb48267

## Remaining proof gaps / decisions
- This establishes scoped RED→GREEN of G4-VAL-03 and G4-SCHEMA-04. It is not a separate independent code reviewer attestation.
- Cross target metadata verification requires target drawable G4 mesh metadata (including a readable triangle topology); failure is fail-closed and may reject legitimately configured target meshes that lack that metadata. Test broader target models before release.
- UI click/edit concurrency and multiple canceling edits within a single UI frame remain unverified; Host desktop input permission previously denied.
- G4 release remains HOLD and G5 T2 Native writer remains HOLD pending separate independent G4 security review, UI/revision transaction proof and agreed G5 CAS contract.
- All user source changes stay in beta working tree. No commit, push, merge or deploy.
