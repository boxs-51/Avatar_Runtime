# G4 Local mesh/rig/topology readback — 2026-10-23

Workspace beta; baseline 2d2fccfaeece6ec686ca09e2e44ae4230574100c.
Status: CODE_PRESENT / PYTHON_SYNTHETIC_GREEN / WINDOWS_RELEASE_BUILD_PASS.
Release HOLD. No commit, push, merge or deploy.

## Implemented
Five trusted-local read operations: mesh.topology.inspect, mesh.vertices.page,
mesh.triangles.page, rig.local.inspect and rig.entries.page.
Arguments are explicit drawable, start/limit (max 4), and rig kind (when used).
Native uses model generation, bounded index/geometry and active Rig V4 signature.
Response classification LOCAL_PRIVATE. No mutation, file export or public SDK ABI changes.

## Evidence
Initial RED-first suite 4 tests FAIL due to missing operations, control test PASS.
After implementation: new synthetic fixture 5 tests and 10 subtests PASS;
affected regression 43 tests and 82 subtests PASS.
Fresh Windows Release avatar_studio build PASS with warnings.
Root installed Release CTest 10/10 PASS; other targets not all freshly rebuilt.
Diff check PASS; public ABI20 paths remain unchanged.

## Unverified
Physical in-process reload while the Studio window remains open, UI/Agent
concurrent edit/read race, cross-request editor revision/coherent frame snapshot,
stress/long-run, independently replicated review and complete public-output
privacy certification. The separate Native *process restart* case is now
verified in the scoped matrix below; it does not prove in-process hot reload.
Rig editor revision is unavailable: model generation does not lock UI edits
across separate reads. Do not infer a frame acceptance fence.
F02/F03/F04/F05/F06/F07 existing gates remain unchanged. Release HOLD.


## Native Windows physical E2E checkpoint — same day

- Harness: scripts/test_studio_g4_native_live.py
- Workspace beta; baseline 2d2fccfaeece6ec686ca09e2e44ae4230574100c,
  plus the uncommitted G4 source/test changes.
- Release avatar_studio binary rebuilt from the G4 Native handler; previous
  build succeeded with existing MSVC warnings.
- Fixture: an existing Owner-local private Model3 manifest, opened only inside
  two distinct short-lived Native Studio test processes. No manifest/model
  asset, identifying path, private result values, image or screenshot is
  published. Test output folders and native logs remain in ignored
  build/studio-user storage. Only in-memory Rig V4 selection/one anchor is
  changed to exercise a populated rig.entries.page; no save/apply/deploy.
- Physical result: 37 categorical case assertions PASS in two Native
  processes, with full per-case public-safe classification stored only in
  local ignored test output.
  * 5/5 new V2 operation successes in each process (10 total).
  * 5/5 malformed direct Native opcode requests rejected in each process
    (10 total), with healthy readback afterward.
  * 5/5 direct Native stale-epoch opcodes rejected after process restart.
  * Fresh 63-bit generation differs after restart; V2 old-generation read
    rejects with STALE_MODEL; fresh generation reads pass.
  * Process startup, selection and in-memory rig setup controls pass.
- Harness-only debugging before final PASS: initial script run had a missing
  Python package root; the next run imported the same V2 module by two names,
  producing a distinct ToolkitV2Error class identity. Both were corrected
  without changing native/product logic; final run PASS.
- Additional affected regression on the same working tree: 53 Python tests
  PASS and 82 subtests PASS. Public SDK ABI20 tracked paths unmodified.
- This is OWNER-LOCAL PHYSICAL test evidence, not a public synthetic MOC3
  fixture, independent review, OS security boundary, renderer frame
  acceptance or Studio release qualification. G4 release gate stays HOLD.
